// SPDX-License-Identifier: MIT
// The bounded silent capture (examples/s2_bench/SilentCapture.h) through the real firmware
// assembly, owner, SDO and NMT services, TWAI adapter and tap, against a synthetic driver, two
// synthetic drives and a synthetic silent GPIO. Times, frames and failures are test inputs with no
// physical claim: a passing case here shows what the software hands to the SDK and the GPIO
// driver, never what reaches the wire or the transceiver.
#include "SilentCapture.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
const char* scenario = "";
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d [%s]: %s\n", __FILE__, __LINE__, scenario, #c);            \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

const char REVISION[] = "0123456789abcdef0123456789abcdef01234567";
const char IMAGE[] = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";

M::Frame frame(uint32_t id, uint8_t count, uint8_t first = 0) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = count;
    for (unsigned i = 0; i < count; ++i)
        value.data[i] = uint8_t(first + i);
    return value;
}
struct Console {
    uint64_t now = 1000000;
    std::string output;
    static bool clock(void* context, uint64_t& out) {
        out = static_cast<Console*>(context)->now;
        return true;
    }
    static bool write(void* context, uint8_t byte) {
        static_cast<Console*>(context)->output.push_back(char(byte));
        return true;
    }
    static bool read(void*, uint8_t&) { return false; }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.context = this;
        value.nowUs = clock;
        value.readByte = read;
        value.writeByte = write;
        return value;
    }
};
// Two drives behind a one-frame controller, as the real shim presents them. Every submission
// is recorded: the scenarios count what reached this boundary, which is the SDK's.
struct Driver {
    Console* console = nullptr;
    E::TwaiEvents pending;
    O::ReceivedFrame incoming[64];
    unsigned head = 0, count = 0;
    std::vector<M::Frame> sent;
    unsigned starts = 0, stops = 0;
    uint32_t epoch = 0, serial = 0;
    bool live = false, active = false, replied = false;
    bool holdReport = false;   // Reply first; the final TX report waits for releaseReport.
    bool uncertainNext = false; // Next submission returns FAILED_UNCERTAIN.
    bool answer = true;        // Reply to SDO requests.
    M::Frame current;
    static Driver& self(void* context) { return *static_cast<Driver*>(context); }
    static bool start(void* context, const E::TwaiConfig&, uint32_t epoch) {
        Driver& d = self(context);
        ++d.starts;
        d.epoch = epoch;
        d.live = true;
        return true;
    }
    static bool stop(void* context) {
        Driver& d = self(context);
        ++d.stops;
        d.live = d.active = false;
        d.count = d.head = 0;
        d.pending = E::TwaiEvents();
        return true;
    }
    static E::TwaiSubmit submit(void* context, const M::Frame& value, uint32_t serial) {
        Driver& d = self(context);
        CHECK(d.live && !d.active);
        if (d.count)
            return E::TwaiSubmit::DEFERRED;
        d.sent.push_back(value);
        if (d.uncertainNext) {
            d.uncertainNext = false;
            return E::TwaiSubmit::FAILED_UNCERTAIN;
        }
        d.current = value;
        d.active = true;
        d.replied = false;
        d.serial = serial;
        return E::TwaiSubmit::SENT;
    }
    static E::TwaiEvents events(void* context) {
        Driver& d = self(context);
        E::TwaiEvents copy = d.pending;
        copy.rxPending = d.count != 0;
        d.pending = E::TwaiEvents();
        return copy;
    }
    static bool receive(void* context, O::ReceivedFrame& output, uint64_t cutoffUs) {
        Driver& d = self(context);
        if (!d.count || (d.incoming[d.head].receiveBoundsKnown &&
                         d.incoming[d.head].receivedLatestUs > cutoffUs))
            return false;
        output = d.incoming[d.head];
        d.head = (d.head + 1) % 64;
        --d.count;
        return true;
    }
    E::TwaiDriver callbacks() {
        E::TwaiDriver value;
        value.context = this;
        value.start = start;
        value.stop = stop;
        value.submit = submit;
        value.events = events;
        value.receive = receive;
        return value;
    }
    void push(const M::Frame& value, uint64_t lo, uint64_t hi) {
        CHECK(count < 64);
        O::ReceivedFrame& next = incoming[(head + count++) % 64];
        next = O::ReceivedFrame();
        next.frame = value;
        next.controllerEpoch = epoch;
        next.receiveBoundsKnown = true;
        next.receivedEarliestUs = lo;
        next.receivedLatestUs = hi;
    }
    void push(const M::Frame& value) { push(value, console->now - 400, console->now - 100); }
    void releaseReport() {
        CHECK(active);
        pending.txDone = pending.transmitted = true;
        pending.serial = serial;
        active = false;
    }
    // One completed exchange per turn: the reply, and the final TX report unless held.
    void deliver() {
        if (!active)
            return;
        if (answer && !replied && current.id > 0x600 && current.id <= 0x67f && current.data[0] != 0x80) {
            M::Frame reply = frame(0x580 + (current.id - 0x600), 8);
            std::memcpy(reply.data + 1, current.data + 1, 3);
            reply.data[0] = (current.data[0] & 0xe0) == 0x40 ? 0x4b : 0x60;
            push(reply);
            replied = true;
        }
        if (!holdReport)
            releaseReport();
    }
};
// The silent GPIO: records each request with the gate state at that moment.
struct Pin {
    struct Call {
        int gpio;
        bool high, gateClosed;
    };
    std::vector<Call> calls;
    unsigned failMask = 0; // Bit n-1 set: call n is refused.
    const E::SoakTwaiTap* tap = nullptr;
    static bool drive(void* context, int gpio, bool high) {
        Pin& p = *static_cast<Pin*>(context);
        p.calls.push_back(Call{gpio, high, p.tap->transmitClosed()});
        return ((p.failMask >> (p.calls.size() - 1)) & 1u) == 0;
    }
};
struct Rig {
    C::SdoClient services;
    Console console;
    Driver driver;
    Pin pin;
    E::SoakTwaiTap tap;
    E::FirmwareProbe firmware;
    E::SilentCapture capture;
    unsigned samples = 0;
    O::SequenceId sequence[2];
    uint32_t operation = 0;
    static void sample(void* context, E::CaptureSample& output) {
        Rig& r = *static_cast<Rig*>(context);
        ++r.samples;
        output.counters = true;
        output.tec = 3 + r.samples;
        output.rec = 7;
    }
    static E::CaptureTransceiver transceiver(Pin& pin) {
        E::CaptureTransceiver value;
        value.gpio = 18;
        value.level = E::SilentLevel::HIGH;
        value.pin.context = &pin;
        value.pin.drive = Pin::drive;
        return value;
    }
    static E::CaptureCallbacks sampling(Rig* rig) {
        E::CaptureCallbacks value;
        value.context = rig;
        value.sample = sample;
        return value;
    }
    Rig()
        : tap(driver.callbacks()), firmware(services, tap.driver(), console.callbacks()),
          capture(firmware, services, tap, transceiver(pin), sampling(this), REVISION, IMAGE) {
        driver.console = &console;
        pin.tap = &tap;
        E::FirmwareConfig config;
        config.configured = true;
        config.controller.txPin = 17;
        config.controller.rxPin = 16;
        config.controller.bitrate = 1000000;
        config.controller.sdkTxQueueDepth = 1;
        config.controller.rxQueueDepth = 16;
        CHECK(firmware.begin(config));
        for (unsigned node = 1; node <= 2; ++node) {
            C::SdoChannel channel;
            channel.binding.logicalAxisId = node;
            channel.binding.endpoint.profile = 1;
            channel.binding.endpoint.address = node;
            channel.binding.bindingGeneration = channel.binding.configurationGeneration = 1;
            channel.clientToServerId = 0x600 + node;
            channel.serverToClientId = 0x580 + node;
            CHECK(services.configure(uint8_t(node - 1), channel, true, true));
            C::NetworkRoute route;
            route.binding = channel.binding;
            route.generation = 1;
            route.heartbeatId = 0x700 + node;
            route.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), route, C::NetworkObserver(), console.now));
        }
        heartbeats();
        turn();
        for (unsigned node = 1; node <= 2; ++node)
            CHECK(firmware.owner().beginSequence(services.channel(uint8_t(node - 1))->binding,
                                                 uint8_t(node), console.now, sequence[node - 1]));
    }
    void heartbeats() {
        for (unsigned node = 1; node <= 2; ++node) {
            M::Frame beat = frame(0x700 + node, 1);
            beat.data[0] = 0x7f; // Pre-operational.
            driver.push(beat);
        }
    }
    void turn() {
        console.now += 1000;
        driver.deliver();
        CHECK(firmware.service());
        capture.service(console.now);
    }
    void turns(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            turn();
    }
    C::SdoTransfer transfer(unsigned node, bool write, uint16_t index = 0x6041) {
        C::SdoTransfer t;
        t.channel = *services.channel(uint8_t(node - 1));
        t.operation = operation;
        t.request = t.step = 1;
        t.index = index;
        t.size = 2;
        t.direction = write ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
        if (write) {
            t.data[0] = 0x34;
            t.data[1] = 0x12;
        } else {
            t.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        }
        t.queueTimeoutUs = t.responseTimeoutUs = 50000;
        t.deadlineUs = console.now + 500000;
        return t;
    }
    // One ordinary request on its own new operation, admitted at the current time.
    O::RequestId admit(unsigned node, bool write, uint16_t index = 0x6041) {
        ++operation;
        O::CanBusOwner& owner = firmware.owner();
        CHECK(owner.beginOperation(sequence[node - 1], operation, console.now + 500000, console.now));
        O::BusRequest request;
        request.sequence = sequence[node - 1];
        request.transfer = transfer(node, write, index);
        O::RequestId id;
        CHECK(owner.admit(request, console.now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    C::NmtRequestId nmt(unsigned node, uint32_t request) {
        C::NmtTransfer work;
        work.command = C::NmtCommand::START;
        work.target = services.node(uint8_t(node - 1))->route;
        work.bus = work.target.binding.endpoint.bus;
        work.request = request;
        work.deadlineUs = console.now + 500000;
        work.queueTimeoutUs = 50000;
        C::NmtRequestId id;
        CHECK(firmware.owner().admitNmt(work, console.now, id));
        return id;
    }
    E::CaptureAdmission begin(uint32_t id, uint32_t seconds = 2, uint16_t frames = 16) {
        E::CapturePlan plan;
        plan.id = id;
        plan.seconds = seconds;
        plan.frames = frames;
        return capture.begin(plan, console.now);
    }
    // Run to the record's end and return its formatted lines.
    std::vector<std::string> lines() const {
        std::vector<std::string> out;
        char text[E::CAPTURE_LINE_BYTES];
        for (unsigned cursor = 0; capture.formatLine(cursor, text, sizeof text); ++cursor)
            out.push_back(text);
        return out;
    }
};
std::unique_ptr<Rig> rig() { return std::unique_ptr<Rig>(new Rig); }
bool startsWith(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }
std::string field(const std::string& line, const std::string& key) {
    const std::string mark = " " + key + "=";
    const size_t at = line.find(mark);
    CHECK(at != std::string::npos);
    const size_t begin = at + mark.size();
    const size_t end = line.find_first_of(" ;\n", begin);
    return line.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}
unsigned long long number(const std::string& line, const std::string& key) {
    return std::strtoull(field(line, key).c_str(), nullptr, 10);
}
const std::string& tagged(const std::vector<std::string>& lines, const char* tag, unsigned which = 0) {
    const bool paired = !std::strcmp(tag, "CAP_CTRL ") || !std::strcmp(tag, "CAP_OWNER ");
    for (const std::string& line : lines)
        if (startsWith(line, tag) && (!paired || number(line, "which") == which))
            return line;
    std::fprintf(stderr, "[%s] no %s line\n", scenario, tag);
    std::abort();
}
// An SDO terminal whose frame never crossed the SDK boundary: the adapter queued it (so
// controllerAccepted is the adapter's local acceptance), the closed gate refused it, and the
// owner withdrew it as proved unsent, with no possible effect.
void provedUnsent(const O::Completion* done) {
    CHECK(done && done->service.outcome == C::SdoOutcome::TRANSPORT_ERROR &&
          !done->service.transmissionPossible && !done->service.effectPossible &&
          !done->service.effectUncertain && !done->service.transmissionConfirmed);
}

// ---------------------------------------------------------------- startup and plan checks
void startupIsPassive() {
    scenario = "startup";
    std::unique_ptr<Rig> r = rig();
    const size_t sent = r->driver.sent.size();
    r->turns(50);
    CHECK(r->pin.calls.empty() && r->driver.sent.size() == sent && !r->tap.transmitClosed());
    CHECK(!r->capture.active() && !r->capture.record() && !r->capture.blocking());
    CHECK(!r->capture.requestStop() && !r->capture.release());
    char text[E::CAPTURE_LINE_BYTES] = "untouched";
    CHECK(!r->capture.formatLine(0, text, sizeof text) && std::strcmp(text, "untouched") == 0);
}
void planIsChecked() {
    scenario = "plan";
    std::unique_ptr<Rig> r = rig();
    CHECK(r->begin(0) == E::CaptureAdmission::INVALID);
    CHECK(r->begin(1, 0) == E::CaptureAdmission::INVALID);
    CHECK(r->begin(1, E::CAPTURE_SECONDS + 1) == E::CaptureAdmission::INVALID);
    CHECK(r->begin(1, 1, 0) == E::CaptureAdmission::INVALID);
    CHECK(r->begin(1, 1, E::CAPTURE_FRAMES + 1) == E::CaptureAdmission::INVALID);
    E::CapturePlan plan;
    plan.id = plan.seconds = 1;
    plan.frames = 1;
    CHECK(r->capture.begin(plan, r->firmware.owner().lastTimeUs() - 1) == E::CaptureAdmission::INVALID);
    CHECK(r->pin.calls.empty() && !r->tap.transmitClosed());
    CHECK(r->begin(5, 1, 1) == E::CaptureAdmission::ACCEPTED);
    CHECK(r->begin(6) == E::CaptureAdmission::BUSY);
    r->turns(1001);
    CHECK(r->capture.record() && r->capture.record()->terminal);
    CHECK(r->begin(6) == E::CaptureAdmission::BUSY); // Held until release.
    CHECK(r->capture.release());
    CHECK(r->begin(5) == E::CaptureAdmission::INVALID && r->begin(4) == E::CaptureAdmission::INVALID);
    CHECK(r->begin(6) == E::CaptureAdmission::ACCEPTED);
}

// ---------------------------------------------------------------- entry refused on evidence
// Ordinary work queued, then active: refused before any GPIO or gate change, and the work then
// completes normally, untouched by the refusal.
void entryRefusedWhileWorkQueuedOrActive() {
    scenario = "entry-work";
    std::unique_ptr<Rig> r = rig();
    const size_t sent = r->driver.sent.size();
    const O::RequestId id = r->admit(1, true, 0x2500);
    CHECK(r->begin(1) == E::CaptureAdmission::WORK); // Queued in the owner.
    CHECK(r->pin.calls.empty() && !r->tap.transmitClosed() && !r->capture.record());
    r->driver.answer = false;
    r->console.now += 1000;
    CHECK(r->firmware.service()); // Submitted: now active, awaiting its reply.
    CHECK(r->driver.sent.size() == sent + 1 && r->driver.active);
    CHECK(r->begin(1) == E::CaptureAdmission::WORK);
    CHECK(r->pin.calls.empty() && !r->tap.transmitClosed());
    r->driver.answer = true;
    r->turns(3);
    const O::Completion* done = r->firmware.owner().result(id);
    CHECK(done && done->service.outcome == C::SdoOutcome::SUCCESS && done->service.transmissionConfirmed);
    CHECK(r->begin(1) == E::CaptureAdmission::ACCEPTED); // The retained result does not block.
}
// The reply overtook its final TX report: the request is terminal and nothing is queued, yet the
// owner cannot prove the frame retired. Unknown retirement refuses; the report then admits.
void entryRefusedWhileTxReportPending() {
    scenario = "entry-report";
    std::unique_ptr<Rig> r = rig();
    r->driver.holdReport = true;
    const O::RequestId id = r->admit(2, false);
    r->turns(4);
    const O::Completion* done = r->firmware.owner().result(id);
    CHECK(done && done->service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(r->firmware.owner().resources().queued == 0 && r->firmware.owner().resources().active == 0);
    CHECK(!r->firmware.owner().localTxRetired(1));
    CHECK(r->begin(1) == E::CaptureAdmission::UNRETIRED);
    CHECK(r->pin.calls.empty() && !r->tap.transmitClosed());
    r->driver.holdReport = false;
    r->driver.releaseReport();
    r->turns(2);
    CHECK(r->firmware.owner().localTxRetired(1) && r->tap.sdkRetired());
    CHECK(r->begin(1) == E::CaptureAdmission::ACCEPTED);
}
// The server answered an expedited-only upload with a segmented initiation, so the client aborted
// it (CLIENT_ABORT) and staged the abort frame as cleanup. While the driver defers that abort
// (captured RX not yet routed), the request is terminal, nothing is queued or active, and the tap
// holds no SDK frame: only the owner's per-slot evidence shows the staged abort, and entry must
// still refuse. Once the abort has left and been reported, entry is admitted.
void entryRefusedWhileCleanupAbortStaged() {
    scenario = "entry-cleanup";
    std::unique_ptr<Rig> r = rig();
    r->driver.answer = false;
    ++r->operation;
    O::CanBusOwner& owner = r->firmware.owner();
    CHECK(owner.beginOperation(r->sequence[0], r->operation, r->console.now + 500000, r->console.now));
    O::BusRequest request;
    request.sequence = r->sequence[0];
    request.transfer = r->transfer(1, false);
    request.transfer.allowSegmentedUpload = false;
    O::RequestId id;
    CHECK(owner.admit(request, r->console.now, id) == O::BusAdmission::ACCEPTED);
    r->turns(3); // Sent and reported; no reply yet.
    CHECK(r->driver.sent.size() && !r->driver.active && r->tap.sdkRetired());
    M::Frame initiation = frame(0x581, 8);
    initiation.data[0] = 0x41;
    std::memcpy(initiation.data + 1, r->driver.sent.back().data + 1, 3);
    initiation.data[4] = 2;
    r->driver.push(initiation);
    // Behind it, a frame that cannot be routed for 30 ms: the driver defers every submission.
    r->driver.push(frame(0x701, 1, 0x7f), r->console.now + 29000, r->console.now + 30000);
    r->turn();
    const O::Completion* done = owner.result(id);
    CHECK(done && done->service.outcome == C::SdoOutcome::CLIENT_ABORT && done->service.clientAbortQueued);
    const O::OwnerResources resources = r->firmware.owner().resources();
    CHECK(resources.queued == 0 && resources.active == 0 && !resources.nmtTxPending);
    CHECK(r->tap.sdkRetired() && !r->firmware.owner().localTxRetired(0));
    CHECK(r->begin(1) == E::CaptureAdmission::UNRETIRED);
    CHECK(r->pin.calls.empty() && !r->tap.transmitClosed());
    const size_t sent = r->driver.sent.size();
    r->turns(40); // The deferring frame is routed; the abort leaves and is reported.
    CHECK(r->driver.sent.size() == sent + 1 && r->driver.sent.back().data[0] == 0x80);
    CHECK(r->firmware.owner().localTxRetired(0) && r->tap.sdkRetired());
    CHECK(r->services.unresolved(0)); // The abort does not settle the route: still ambiguous.
    CHECK(r->begin(1) == E::CaptureAdmission::ACCEPTED);
    CHECK(r->services.unresolved(0)); // Nor does entry.
}
// The driver reported the submission uncertain: the adapter and owner close it, but the SDK may
// still hold the frame. The tap's own evidence refuses until a confirmed stop.
void entryRefusedAfterUncertainSubmission() {
    scenario = "entry-uncertain";
    std::unique_ptr<Rig> r = rig();
    r->driver.uncertainNext = true;
    const O::RequestId id = r->admit(1, false);
    r->turns(3);
    const O::Completion* done = r->firmware.owner().result(id);
    CHECK(done && done->service.transmissionPossible);
    CHECK(r->firmware.owner().localTxRetired(0) && r->firmware.owner().localTxRetired(1));
    CHECK(!r->tap.sdkRetired());
    CHECK(r->begin(1) == E::CaptureAdmission::UNRETIRED);
    CHECK(r->pin.calls.empty() && !r->tap.transmitClosed());
}
// NMT pending, then cancelled after its frame was handed over: the intent is closed but its TX
// report is still owed, so retirement is unknown. Refused, refused, then admitted after the report.
void entryRefusedWhileNmtPending() {
    scenario = "entry-nmt";
    std::unique_ptr<Rig> r = rig();
    const C::NmtRequestId id = r->nmt(1, 77);
    CHECK(r->begin(1) == E::CaptureAdmission::WORK);
    r->driver.holdReport = true;
    r->turns(3);
    CHECK(r->driver.active && r->driver.sent.back().id == 0);
    CHECK(r->begin(1) == E::CaptureAdmission::WORK);
    CHECK(r->firmware.owner().cancelNmt(id, r->console.now));
    r->turn();
    CHECK(!r->firmware.owner().resources().nmtPending && r->firmware.owner().resources().nmtTxPending);
    CHECK(r->begin(1) == E::CaptureAdmission::UNRETIRED);
    CHECK(r->pin.calls.empty() && !r->tap.transmitClosed());
    r->driver.holdReport = false;
    r->driver.releaseReport();
    r->turns(3);
    const C::NmtCompletion* done = r->firmware.owner().nmtResult(id);
    CHECK(done && done->transmissionPossible && !r->firmware.owner().resources().nmtTxPending);
    const C::NmtCompletion copy = *done;
    CHECK(r->begin(1) == E::CaptureAdmission::ACCEPTED);
    CHECK(std::memcmp(&copy, r->firmware.owner().nmtResult(id), sizeof copy) == 0); // Retained.
}
void entryRefusedForControllerAndReads() {
    scenario = "entry-controller";
    {
        std::unique_ptr<Rig> r = rig();
        L::ReadTarget target;
        target.channel = *r->services.channel(0);
        target.nodeGeneration = r->services.node(0)->route.generation;
        CHECK(r->firmware.owner().releaseSequence(r->sequence[0]));
        CHECK(r->firmware.app().select(target));
        L::ReadOptions options;
        options.queueTimeoutUs = options.responseTimeoutUs = 50000;
        CHECK(r->firmware.app().requestIdentity(r->console.now, r->console.now + 900000, options));
        CHECK(r->begin(1) == E::CaptureAdmission::WORK); // A typed read prepared, nothing sent yet.
        CHECK(r->firmware.app().cancelRead(r->console.now));
    }
    {
        std::unique_ptr<Rig> r = rig();
        O::RecoveryId recovery;
        CHECK(r->firmware.owner().recover(r->console.now, r->console.now + 1000000, recovery));
        CHECK(r->begin(1) == E::CaptureAdmission::CONTROLLER);
        CHECK(r->pin.calls.empty() && !r->tap.transmitClosed());
    }
    {
        std::unique_ptr<Rig> r = rig();
        r->driver.pending.busOff = true;
        r->turn();
        CHECK(r->firmware.owner().resources().inhibited);
        CHECK(r->begin(1) == E::CaptureAdmission::CONTROLLER && r->pin.calls.empty());
    }
    {
        std::unique_ptr<Rig> r = rig();
        CHECK(r->firmware.stop());
        CHECK(r->begin(1) == E::CaptureAdmission::CONTROLLER && r->pin.calls.empty());
    }
}

// ---------------------------------------------------------------- GPIO failures at entry and exit
void silentRefusedThenRestored() {
    scenario = "silent-restored";
    std::unique_ptr<Rig> r = rig();
    r->pin.failMask = 1; // The silent request is refused; the restore is accepted.
    CHECK(r->begin(1) == E::CaptureAdmission::SILENT);
    CHECK(r->pin.calls.size() == 2 && r->pin.calls[0].high && !r->pin.calls[1].high);
    CHECK(r->pin.calls[0].gateClosed && r->pin.calls[1].gateClosed); // Closed before either.
    CHECK(!r->tap.transmitClosed() && !r->capture.latched() && !r->capture.record());
    CHECK(r->capture.level() == E::TransceiverState::NORMAL_REQUESTED);
    const size_t sent = r->driver.sent.size();
    const O::RequestId id = r->admit(1, false);
    r->turns(3);
    CHECK(r->driver.sent.size() == sent + 1); // Ordinary work resumes.
    CHECK(r->firmware.owner().result(id)->service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(r->begin(1) == E::CaptureAdmission::ACCEPTED); // A refused entry consumed no identity.
}
void silentAndRestoreRefused() {
    scenario = "silent-latched";
    std::unique_ptr<Rig> r = rig();
    r->pin.failMask = 3;
    CHECK(r->begin(1) == E::CaptureAdmission::SILENT);
    CHECK(r->pin.calls.size() == 2 && r->capture.level() == E::TransceiverState::FAILED);
    CHECK(r->tap.transmitClosed() && r->capture.latched() && !r->capture.record());
    CHECK(std::strcmp(r->capture.blocking(), "closed") == 0);
    const size_t sent = r->driver.sent.size();
    const uint32_t refused = r->tap.refused();
    const O::RequestId id = r->admit(1, false);
    r->turns(4);
    CHECK(r->driver.sent.size() == sent && r->tap.refused() == refused + 1);
    provedUnsent(r->firmware.owner().result(id));
    CHECK(!r->services.unresolved(0));
    CHECK(r->begin(2) == E::CaptureAdmission::CLOSED && r->pin.calls.size() == 2);
}
void exitNormalRefused() {
    scenario = "exit-refused";
    std::unique_ptr<Rig> r = rig();
    r->pin.failMask = 2; // The exit's normal request.
    CHECK(r->begin(1, 1) == E::CaptureAdmission::ACCEPTED);
    r->turns(1001);
    const E::CaptureRecord* record = r->capture.record();
    CHECK(record && record->terminal && record->end == E::CaptureEnd::DURATION);
    CHECK(record->exit == E::CaptureExit::FAILED && !record->reopened && r->capture.latched());
    CHECK(r->tap.transmitClosed() && r->capture.level() == E::TransceiverState::FAILED);
    CHECK(r->capture.release() && std::strcmp(r->capture.blocking(), "closed") == 0);
    const size_t sent = r->driver.sent.size();
    r->admit(2, false);
    r->turns(4);
    CHECK(r->driver.sent.size() == sent);
    CHECK(r->begin(2) == E::CaptureAdmission::CLOSED);
}

// ---------------------------------------------------------------- every send path is refused
// Each path the owner has to the SDK, tried while a capture collects. Each reaches the tap's
// closed gate, closes provably unsent, and nothing reaches the driver. The capture keeps running.
struct Collecting {
    std::unique_ptr<Rig> r;
    size_t sent;
    uint32_t refused;
    Collecting() : r(rig()) {
        CHECK(r->begin(1, 60, 64) == E::CaptureAdmission::ACCEPTED);
        sent = r->driver.sent.size();
        refused = r->tap.refused();
    }
    void nothingSent(unsigned attempts) {
        CHECK(r->driver.sent.size() == sent && r->tap.refused() == refused + attempts);
        CHECK(r->capture.active() && r->tap.transmitClosed());
    }
};
void refusedOrdinaryUpload() {
    scenario = "send-upload";
    Collecting c;
    const O::RequestId id = c.r->admit(1, false);
    c.r->turns(4);
    provedUnsent(c.r->firmware.owner().result(id));
    c.nothingSent(1);
}
void refusedOrdinaryDownload() {
    scenario = "send-download";
    Collecting c;
    const O::RequestId id = c.r->admit(2, true, 0x2500);
    c.r->turns(4);
    provedUnsent(c.r->firmware.owner().result(id));
    CHECK(!c.r->services.unresolved(1));
    c.nothingSent(1);
}
void refusedUrgentWork() {
    scenario = "send-urgent";
    Collecting c;
    O::CanBusOwner& owner = c.r->firmware.owner();
    ++c.r->operation;
    CHECK(owner.beginUrgentOperation(c.r->sequence[0], c.r->operation, c.r->console.now + 500000,
                                     c.r->console.now));
    O::BusRequest request;
    request.sequence = c.r->sequence[0];
    request.transfer = c.r->transfer(1, true, 0x6040);
    O::RequestId id;
    CHECK(owner.admitUrgent(request, c.r->console.now, id) == O::BusAdmission::ACCEPTED);
    c.r->turns(4);
    provedUnsent(owner.result(id));
    c.nothingSent(1);
}
void refusedBestEffortStop() {
    scenario = "send-best-effort";
    Collecting c;
    C::SdoTransfer stop = c.r->transfer(2, true, 0x6040);
    stop.operation = stop.request = stop.step = 901;
    stop.data[0] = 0x0b;
    stop.data[1] = 0;
    O::RequestId id;
    CHECK(c.r->firmware.owner().admitBestEffortStop(stop, c.r->console.now, id) ==
          O::BusAdmission::ACCEPTED);
    c.r->turns(4);
    provedUnsent(c.r->firmware.owner().result(id));
    c.nothingSent(1);
}
void refusedNmt() {
    scenario = "send-nmt";
    Collecting c;
    const C::NmtRequestId id = c.r->nmt(2, 88);
    c.r->turns(4);
    const C::NmtCompletion* done = c.r->firmware.owner().nmtResult(id);
    CHECK(done && !done->transmissionPossible && !done->transmissionConfirmed);
    c.nothingSent(1);
}
void refusedDrainMarker() {
    scenario = "send-drain-marker";
    Collecting c;
    // An unsolicited server frame makes channel 0 ambiguous; the capture neither caused nor ends it.
    c.r->driver.push(frame(0x581, 8, 0x43));
    c.r->turns(2);
    CHECK(c.r->services.unresolved(0));
    C::SdoTransfer marker = c.r->transfer(1, false, 0x2000);
    marker.operation = marker.request = marker.step = 900;
    marker.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    marker.allowSegmentedUpload = false;
    C::SdoDrainPremises premises;
    premises.soleClient = premises.orderedServer = true;
    O::RequestId id;
    CHECK(c.r->firmware.owner().admitDrainMarker(marker, premises, c.r->console.now, id) ==
          O::BusAdmission::ACCEPTED);
    c.r->turns(4);
    provedUnsent(c.r->firmware.owner().result(id));
    CHECK(c.r->services.unresolved(0)); // Still ambiguous: nothing resolved it.
    c.nothingSent(1);
}
// A request withdrawn before it could leave queues no client abort: nothing to clean up.
void refusedCleanupAfterCancel() {
    scenario = "send-cancel";
    Collecting c;
    const O::RequestId id = c.r->admit(1, true, 0x2500);
    CHECK(c.r->firmware.owner().cancel(id, c.r->console.now) == O::Cancel::CANCELLED);
    c.r->turns(4);
    CHECK(c.r->driver.sent.size() == c.sent && !c.r->services.unresolved(0));
}
// The choke point itself: every frame class handed to the tap while closed is refused before the
// inner driver, including the classes this library never produces (SYNC, PDO, extended, RTR).
void refusedAtTheDriverBoundary() {
    scenario = "send-boundary";
    Collecting c;
    const E::TwaiDriver d = c.r->tap.driver();
    M::Frame cases[] = {frame(0x601, 8, 0x40), frame(0x602, 8, 0x80), frame(0x000, 2, 1),
                        frame(0x080, 0), frame(0x201, 8), frame(0x181, 4), frame(0x701, 1),
                        frame(0x18ff1234, 8), frame(0x605, 0)};
    cases[7].extended = true;
    cases[8].rtr = true;
    cases[8].dlc = 8;
    uint32_t serial = 1000;
    for (const M::Frame& f : cases)
        CHECK(d.submit(d.context, f, ++serial) == E::TwaiSubmit::FAILED_UNSENT);
    c.nothingSent(unsigned(sizeof cases / sizeof cases[0]));
    CHECK(c.r->tap.sdkRetired());
}
// An explicit recovery during a capture ends it as CONTROLLER; the silent level stays requested,
// the gate stays closed, and the new node gets nothing from the old work.
void recoveryEndsTheCapture() {
    scenario = "send-recovery";
    Collecting c;
    O::RecoveryId recovery;
    CHECK(c.r->firmware.owner().recover(c.r->console.now, c.r->console.now + 1000000, recovery));
    c.r->turns(5);
    const E::CaptureRecord* record = c.r->capture.record();
    CHECK(record && record->terminal && record->end == E::CaptureEnd::CONTROLLER);
    CHECK(record->exit == E::CaptureExit::LEFT_SILENT && c.r->capture.latched());
    CHECK(c.r->pin.calls.size() == 1 && c.r->pin.calls[0].high);
    CHECK(c.r->tap.transmitClosed() && c.r->driver.sent.size() == c.sent);
}

// ---------------------------------------------------------------- lifecycle
void finiteExpiryAndExit() {
    scenario = "expiry";
    std::unique_ptr<Rig> r = rig();
    const size_t sent = r->driver.sent.size();
    CHECK(r->begin(3, 2, 16) == E::CaptureAdmission::ACCEPTED);
    const uint64_t begin = r->console.now;
    CHECK(r->pin.calls.size() == 1 && r->pin.calls[0].gpio == 18 && r->pin.calls[0].high &&
          r->pin.calls[0].gateClosed);
    CHECK(r->samples == 1 && r->capture.active() && std::strcmp(r->capture.blocking(), "active") == 0);
    r->turns(1999); // now = begin + 1999 ms: still collecting.
    CHECK(r->capture.active() && r->capture.record() && !r->capture.record()->terminal);
    CHECK(!r->capture.formatLine(0, nullptr, 0));
    r->turn(); // now = begin + 2 s exactly.
    const E::CaptureRecord* record = r->capture.record();
    CHECK(!r->capture.active() && record && record->terminal && record->end == E::CaptureEnd::DURATION);
    CHECK(record->beginUs == begin && record->endUs == begin + 2000000 &&
          record->nominalEndUs == begin + 2000000);
    CHECK(record->exit == E::CaptureExit::NORMAL && record->reopened &&
          record->reopenedUs == record->endUs);
    CHECK(r->pin.calls.size() == 2 && !r->pin.calls[1].high && r->pin.calls[1].gateClosed);
    CHECK(!r->tap.transmitClosed() && r->samples == 2 && r->driver.sent.size() == sent);
    CHECK(std::strcmp(r->capture.blocking(), "held") == 0);
    const std::vector<std::string> lines = r->lines();
    CHECK(lines.size() == E::CAPTURE_CONTROL_LINES + 1);
    CHECK(field(lines[0], "reason") == "duration" && number(lines[0], "stored") == 0);
    CHECK(number(tagged(lines, "CAP_TIME "), "begin") == begin);
    CHECK(field(tagged(lines, "CAP_TIME "), "clock") == "esp_timer_us");
    CHECK(number(tagged(lines, "CAP_LEVEL "), "exit") == 1 && number(tagged(lines, "CAP_LEVEL "), "reopened") == 1);
    CHECK(number(tagged(lines, "CAP_CTRL ", 0), "tec") == 4 && number(tagged(lines, "CAP_CTRL ", 1), "tec") == 5);
    CHECK(field(tagged(lines, "CAP_IMAGE "), "base") == REVISION && field(tagged(lines, "CAP_IMAGE "), "elf") == IMAGE);
    CHECK(startsWith(lines.back(), "END capture cap=3 stored=0 overflow=0 lost=0;"));
    // Repeated exit: nothing more happens.
    CHECK(!r->capture.requestStop());
    r->turns(10);
    CHECK(r->pin.calls.size() == 2 && r->samples == 2 && r->capture.record() == record);
    CHECK(r->capture.release() && !r->capture.release() && !r->capture.record());
    CHECK(!r->capture.blocking() && r->pin.calls.size() == 2);
    // Ordinary work runs after the gate opened.
    const O::RequestId id = r->admit(1, false);
    r->turns(3);
    CHECK(r->driver.sent.size() == sent + 1 &&
          r->firmware.owner().result(id)->service.outcome == C::SdoOutcome::SUCCESS);
}
void explicitStop() {
    scenario = "stop";
    std::unique_ptr<Rig> r = rig();
    CHECK(r->begin(1, 600, 8) == E::CaptureAdmission::ACCEPTED);
    r->turns(5);
    CHECK(r->capture.requestStop() && r->capture.requestStop()); // Still collecting until service.
    r->turn();
    const E::CaptureRecord* record = r->capture.record();
    CHECK(record->terminal && record->end == E::CaptureEnd::STOPPED && !r->capture.requestStop());
    CHECK(r->pin.calls.size() == 2 && !r->tap.transmitClosed());
    CHECK(field(r->lines()[0], "reason") == "stop");
}
void framesAreCopiedAndOverflowCounted() {
    scenario = "overflow";
    std::unique_ptr<Rig> r = rig();
    CHECK(r->begin(1, 10, 3) == E::CaptureAdmission::ACCEPTED);
    const uint32_t ownerRx = r->firmware.owner().stats().rxFrames;
    M::Frame ext = frame(0x18ff1234, 8, 0xa0);
    ext.extended = true;
    M::Frame remote = frame(0x281, 0);
    remote.rtr = true;
    remote.dlc = 4;
    r->driver.push(ext);
    r->driver.push(remote);
    r->driver.push(frame(0x123, 3, 0x10)); // An identifier nobody configured: diagnostic data.
    r->driver.push(frame(0x701, 1, 0x7f));
    r->driver.push(frame(0x702, 1, 0x7f));
    r->turns(3);
    const E::CaptureRecord* record = r->capture.record();
    CHECK(record->stored == 3 && record->overflow == 2 && record->tapDropped == 0);
    CHECK(r->firmware.owner().stats().rxFrames == ownerRx + 5); // The owner still got every frame.
    CHECK(record->frames[0].id == 0x18ff1234 && (record->frames[0].flags & E::CAPTURE_EXTENDED));
    CHECK((record->frames[1].flags & E::CAPTURE_RTR) && record->frames[1].dlc == 4);
    CHECK(record->frames[2].id == 0x123 && record->frames[2].data[2] == 0x12);
    CHECK(r->capture.requestStop());
    r->turn();
    const std::vector<std::string> lines = r->lines();
    CHECK(lines.size() == E::CAPTURE_CONTROL_LINES + 3 + 1);
    const std::string& first = lines[E::CAPTURE_CONTROL_LINES];
    CHECK(field(first, "id") == "18ff1234" && field(first, "ext") == "1" &&
          field(first, "data") == "a0a1a2a3a4a5a6a7");
    CHECK(field(lines[E::CAPTURE_CONTROL_LINES + 1], "data") == "-" &&
          field(lines[E::CAPTURE_CONTROL_LINES + 1], "rtr") == "1");
    CHECK(number(lines[0], "overflow") == 2 && number(lines[0], "lost") == 0);
    CHECK(startsWith(lines.back(), "END capture cap=1 stored=3 overflow=2 lost=0;"));
}
// More copies between two capture services than the tap ring holds: the excess is never seen,
// counted as tapDropped, and the record says lost.
void tapRingLossIsReported() {
    scenario = "tap-loss";
    std::unique_ptr<Rig> r = rig();
    CHECK(r->begin(1, 10, 128) == E::CaptureAdmission::ACCEPTED);
    for (unsigned turn = 0; turn < 6; ++turn) {
        for (unsigned i = 0; i < 8; ++i)
            r->driver.push(frame(0x181, 2, uint8_t(i)));
        r->console.now += 1000;
        CHECK(r->firmware.service()); // No capture service: copies accumulate in the ring.
    }
    r->capture.service(r->console.now);
    const E::CaptureRecord* record = r->capture.record();
    CHECK(record->stored == E::ReceiveRing::CAPACITY && record->tapDropped == 48 - E::ReceiveRing::CAPACITY);
    CHECK(r->capture.requestStop());
    r->turn();
    CHECK(number(r->lines()[0], "lost") == 1 && number(r->lines()[0], "tapDropped") == 16);
}
// A frame still in the driver FIFO when the capture begins is copied afterwards and marked pre; a
// frame that arrived after the nominal end, copied in the end turn, is marked post; one copied
// after the end turn is not in the record at all, while the owner still dispatches it.
void lateReceiveBoundaries() {
    scenario = "late-rx";
    std::unique_ptr<Rig> r = rig();
    const uint64_t before = r->console.now;
    r->driver.push(frame(0x701, 1, 0x7f), before - 900, before - 500); // Waiting in the FIFO.
    CHECK(r->begin(1, 1, 16) == E::CaptureAdmission::ACCEPTED);
    r->turn();
    const E::CaptureRecord* record = r->capture.record();
    CHECK(record->stored == 1 && (record->frames[0].flags & E::CAPTURE_PRE) && record->pre == 1);
    r->turns(998); // now = nominal - 1 ms.
    CHECK(r->capture.active());
    const uint64_t nominal = record->nominalEndUs;
    // Received after the nominal end but before the end turn, 1 ms late, processes it.
    r->console.now += 2000;
    r->driver.push(frame(0x702, 1, 0x7f), nominal + 200, nominal + 600);
    CHECK(r->firmware.service());
    r->capture.service(r->console.now);
    CHECK(record->terminal && record->stored == 2 && (record->frames[1].flags & E::CAPTURE_POST));
    const uint32_t ownerRx = r->firmware.owner().stats().rxFrames;
    r->driver.push(frame(0x701, 1, 0x7f));
    r->turns(2);
    CHECK(record->stored == 2 && r->firmware.owner().stats().rxFrames == ownerRx + 1);
    O::ReceivedFrame copied;
    CHECK(!r->tap.takeReceived(copied)); // The tap stopped copying at the end turn.
    const std::vector<std::string> lines = r->lines();
    CHECK(field(lines[E::CAPTURE_CONTROL_LINES], "pre") == "1" &&
          field(lines[E::CAPTURE_CONTROL_LINES + 1], "post") == "1");
    CHECK(number(lines[0], "pre") == 1 && number(lines[0], "post") == 1);
}
// Work admitted directly during a capture, still queued when the capture ends, cannot leave
// across the exit: the gate stays closed until the owner is idle, the frame meets it, and only
// then does the gate open. The same holds for a write queued before entry: entry refused it.
void queuedWriteCannotLeaveAcrossExit() {
    scenario = "exit-queued-write";
    std::unique_ptr<Rig> r = rig();
    const size_t sent = r->driver.sent.size();
    CHECK(r->begin(1, 60, 8) == E::CaptureAdmission::ACCEPTED);
    r->turns(3);
    const O::RequestId id = r->admit(1, true, 0x2500);
    CHECK(r->capture.requestStop());
    r->console.now += 1000;
    r->capture.service(r->console.now); // Ends before the owner has even handed the write over.
    const E::CaptureRecord* record = r->capture.record();
    CHECK(record->terminal && record->exit == E::CaptureExit::NORMAL && !record->reopened);
    CHECK(r->tap.transmitClosed() && std::strcmp(r->capture.blocking(), "held") == 0);
    r->turns(5);
    provedUnsent(r->firmware.owner().result(id));
    CHECK(r->driver.sent.size() == sent); // The write never reached the driver.
    CHECK(!r->tap.transmitClosed() && record->reopened && record->reopenedUs > record->endUs);
    const O::RequestId next = r->admit(2, false);
    r->turns(3);
    CHECK(r->driver.sent.size() == sent + 1 &&
          r->firmware.owner().result(next)->service.outcome == C::SdoOutcome::SUCCESS);
}
// A controller fault during a capture ends it; no normal level is requested on the failed path.
void controllerFaultEndsTheCapture() {
    scenario = "fault";
    std::unique_ptr<Rig> r = rig();
    CHECK(r->begin(1, 60, 8) == E::CaptureAdmission::ACCEPTED);
    r->turns(2);
    r->driver.pending.busOff = true;
    r->turn();
    const E::CaptureRecord* record = r->capture.record();
    CHECK(record->terminal && record->end == E::CaptureEnd::CONTROLLER);
    CHECK(record->exit == E::CaptureExit::LEFT_SILENT && r->capture.latched());
    CHECK(r->pin.calls.size() == 1 && r->tap.transmitClosed());
    CHECK(number(tagged(r->lines(), "CAP_CTRL ", 1), "inhibited") == 1);
    CHECK(field(r->lines()[0], "reason") == "controller");
}
// The bench begins with the platform's time, which can be later than the owner's; a clock failure
// then freezes the owner's time below it. The failed controller still ends the capture, at the
// begin time, rather than leaving it collecting for the rest of the boot.
void frozenClockStillEndsTheCapture() {
    scenario = "frozen-clock";
    std::unique_ptr<Rig> r = rig();
    const uint64_t owner = r->firmware.owner().lastTimeUs();
    E::CapturePlan plan;
    plan.id = plan.seconds = 1;
    plan.frames = 4;
    CHECK(r->capture.begin(plan, owner + 5000) == E::CaptureAdmission::ACCEPTED);
    CHECK(r->firmware.stop());
    r->capture.service(owner); // The frozen owner time, below the begin time.
    const E::CaptureRecord* record = r->capture.record();
    CHECK(!r->capture.active() && record->terminal && record->end == E::CaptureEnd::CONTROLLER);
    CHECK(record->endUs == owner + 5000 && record->exit == E::CaptureExit::LEFT_SILENT);
    CHECK(r->pin.calls.size() == 1 && r->tap.transmitClosed());
}
// Retained results and an ambiguous channel survive a refused entry and a whole capture unchanged.
void retainedTerminalsSurvive() {
    scenario = "retained";
    std::unique_ptr<Rig> r = rig();
    const O::RequestId kept = r->admit(2, false);
    r->turns(3);
    const C::NmtRequestId nmt = r->nmt(2, 55);
    r->turns(3);
    r->driver.push(frame(0x581, 8, 0x43)); // Channel 0 becomes ambiguous.
    r->turns(2);
    CHECK(r->services.unresolved(0) && !r->services.unresolved(1));
    O::Completion keptCopy = *r->firmware.owner().result(kept);
    C::NmtCompletion nmtCopy = *r->firmware.owner().nmtResult(nmt);
    const O::RequestId blocking = r->admit(2, false);
    CHECK(r->begin(1) == E::CaptureAdmission::WORK); // A refused entry.
    r->turns(3);
    CHECK(r->firmware.owner().release(blocking));
    CHECK(r->begin(2, 1, 8) == E::CaptureAdmission::ACCEPTED);
    r->turns(1001);
    CHECK(r->capture.record()->terminal && r->capture.release());
    CHECK(std::memcmp(&keptCopy, r->firmware.owner().result(kept), sizeof keptCopy) == 0);
    CHECK(std::memcmp(&nmtCopy, r->firmware.owner().nmtResult(nmt), sizeof nmtCopy) == 0);
    CHECK(r->services.unresolved(0) && !r->services.unresolved(1));
}
// Line shapes and widths: every line fits its buffer with every numeric field at its widest.
void linesFit() {
    scenario = "fit";
    std::unique_ptr<Rig> r = rig();
    CHECK(r->begin(4294967295u, 600, E::CAPTURE_FRAMES) == E::CaptureAdmission::ACCEPTED);
    M::Frame ext = frame(0x1fffffff, 8, 0xf0);
    ext.extended = true;
    r->driver.push(ext); // Values are widened below; the frame gives the CAP_F line its fields.
    r->turn();
    CHECK(r->capture.requestStop());
    r->turn();
    const std::vector<std::string> lines = r->lines();
    const char* const tags[] = {"CAP_REC ", "CAP_TIME ", "CAP_LEVEL ", "CAP_CTRL ", "CAP_CTRL ",
                                "CAP_OWNER ", "CAP_OWNER ", "CAP_IMAGE ", "CAP_F ", "END capture "};
    CHECK(lines.size() == sizeof tags / sizeof tags[0]);
    for (size_t i = 0; i < lines.size(); ++i) {
        CHECK(startsWith(lines[i], tags[i]) && lines[i].back() == '\n');
        CHECK(lines[i].find('\n') == lines[i].size() - 1);
        CHECK(number(lines[i], "cap") == 4294967295u);
        // Widen every field to the widest its type can print: still inside the buffer, with the
        // terminator vsnprintf needs.
        struct Width {
            const char* key;
            size_t digits;
        };
        static const Width widths[] = {
            {"cap", 10},      {"reason", 10},     {"seconds", 10},  {"capacity", 5},   {"stored", 5},
            {"overflow", 10}, {"tapDropped", 10}, {"pre", 5},       {"post", 5},       {"untimed", 5},
            {"txRefused", 10}, {"lost", 10},      {"begin", 20},    {"end", 20},       {"nominal", 20},
            {"gap", 20},      {"clock", 12},      {"gpio", 11},     {"silentHigh", 1}, {"exit", 3},
            {"reopened", 1},  {"at", 20},         {"latched", 1},   {"which", 1},      {"t", 20},
            {"epoch", 10},    {"counters", 1},    {"tec", 10},      {"rec", 10},       {"inhibited", 1},
            {"adapterInhibited", 1}, {"twaiDropped", 10}, {"twaiErrors", 10}, {"twaiFlags", 8},
            {"rx", 10},       {"dropped", 10},    {"unmatched", 10}, {"tx", 10},       {"txRej", 10},
            {"busOff", 10},   {"refused", 10},    {"base", 40},     {"elf", 64},       {"i", 5},
            {"id", 8},        {"ext", 1},         {"rtr", 1},       {"dlc", 3},        {"data", 16},
            {"bounds", 1},    {"exact", 1},       {"lo", 20},       {"hi", 20}};
        size_t widened = lines[i].size() + 1;
        size_t at = 0;
        const size_t prose = lines[i].find(';'); // Text after it is not a field.
        while ((at = lines[i].find('=', at)) != std::string::npos && at < prose) {
            const size_t key = lines[i].rfind(' ', at) + 1;
            const std::string name = lines[i].substr(key, at - key);
            const size_t end = lines[i].find_first_of(" ;\n", ++at);
            const size_t current = end - at;
            size_t widest = 0;
            for (const Width& w : widths)
                if (name == w.key)
                    widest = w.digits;
            CHECK(widest >= current); // Every field has a stated width.
            widened += widest - current;
        }
        CHECK(widened <= size_t(E::CAPTURE_LINE_BYTES));
    }
    char small[16];
    CHECK(r->capture.formatLine(0, small, sizeof small) && small[0] == 0); // Never a cut line.
}
// The tap's own retirement evidence, at its boundary.
void tapRetirementEvidence() {
    scenario = "tap";
    Driver inner;
    Console console;
    inner.console = &console;
    E::SoakTwaiTap tap(inner.callbacks());
    const E::TwaiDriver d = tap.driver();
    E::TwaiConfig config;
    CHECK(d.start(d.context, config, 1) && tap.sdkRetired());
    CHECK(d.submit(d.context, frame(0x601, 8, 0x40), 11) == E::TwaiSubmit::SENT && !tap.sdkRetired());
    inner.pending.txDone = true;
    inner.pending.serial = 12; // Another serial retires nothing.
    d.events(d.context);
    CHECK(!tap.sdkRetired());
    inner.pending.txDone = true;
    inner.pending.serial = 11;
    inner.active = false;
    d.events(d.context);
    CHECK(tap.sdkRetired());
    inner.uncertainNext = true;
    CHECK(d.submit(d.context, frame(0x601, 8, 0x40), 13) == E::TwaiSubmit::FAILED_UNCERTAIN);
    CHECK(!tap.sdkRetired());
    inner.pending.txDone = true;
    inner.pending.serial = 13;
    d.events(d.context);
    CHECK(!tap.sdkRetired()); // No report can retire an uncertain handoff.
    CHECK(d.stop(d.context) && tap.sdkRetired());
    tap.closeTransmit();
    CHECK(d.start(d.context, config, 2));
    const size_t sent = inner.sent.size();
    CHECK(d.submit(d.context, frame(0x601, 8, 0x40), 14) == E::TwaiSubmit::FAILED_UNSENT);
    CHECK(inner.sent.size() == sent && tap.refused() == 1 && tap.sdkRetired());
    E::SoakSubmission taken;
    CHECK(tap.take(taken) && taken.result == E::TwaiSubmit::FAILED_UNSENT && taken.serial == 14);
    tap.openTransmit();
    CHECK(d.submit(d.context, frame(0x601, 8, 0x40), 15) == E::TwaiSubmit::SENT && inner.sent.size() == sent + 1);
}
void names() {
    scenario = "names";
    CHECK(std::strcmp(E::captureAdmissionName(E::CaptureAdmission::UNRETIRED), "unretired") == 0);
    CHECK(std::strcmp(E::captureAdmissionName(E::CaptureAdmission::CLOSED), "closed") == 0);
    CHECK(std::strcmp(E::captureAdmissionName(E::CaptureAdmission(200)), "unknown") == 0);
    CHECK(std::strcmp(E::captureEndName(E::CaptureEnd::STOPPED), "stop") == 0);
}
} // namespace

int main() {
    startupIsPassive();
    planIsChecked();
    entryRefusedWhileWorkQueuedOrActive();
    entryRefusedWhileTxReportPending();
    entryRefusedWhileCleanupAbortStaged();
    entryRefusedAfterUncertainSubmission();
    entryRefusedWhileNmtPending();
    entryRefusedForControllerAndReads();
    silentRefusedThenRestored();
    silentAndRestoreRefused();
    exitNormalRefused();
    // The negative send-path group: one scenario per path to the SDK.
    refusedOrdinaryUpload();
    refusedOrdinaryDownload();
    refusedUrgentWork();
    refusedBestEffortStop();
    refusedNmt();
    refusedDrainMarker();
    refusedCleanupAfterCancel();
    refusedAtTheDriverBoundary();
    recoveryEndsTheCapture();
    finiteExpiryAndExit();
    explicitStop();
    framesAreCopiedAndOverflowCounted();
    tapRingLossIsReported();
    lateReceiveBoundaries();
    queuedWriteCannotLeaveAcrossExit();
    controllerFaultEndsTheCapture();
    frozenClockStillEndsTheCapture();
    retainedTerminalsSurvive();
    linesFit();
    tapRetirementEvidence();
    names();
    std::printf("silent_capture: %u checks passed\n", checks);
    return 0;
}
