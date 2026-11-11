// SPDX-License-Identifier: MIT
// HOST_FAKE only. Typed acquisition and settings use the real owner/session boundary;
// this finite scalar dictionary is synthetic traffic, never device qualification.
#include "ProbeConsole.h"
#include "ProbeSettings.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;
namespace E = MotorControlCANExample;
static unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"

namespace {
constexpr uint64_t DEADLINE = 100000;
constexpr uint64_t AGE = 10000;
constexpr uint8_t STATE =
    uint8_t((1u << unsigned(L::StateBlock::MONITORS)) | (1u << unsigned(L::StateBlock::STATUS)));
L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = value.responseTimeoutUs = 100;
    return value;
}
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
struct Entry {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
// The same scalar, width-checked request/reply fixture as settings_owner_test, narrowed to
// the settings and prerequisites exercised here. An ACK is produced only for a received write.
struct Dictionary {
    Entry values[33] = {
        {0x2100, 0, 2, 1},           {0x2101, 0, 2, 0},      {0x2102, 0, 2, 0},
        {0x6041, 0, 2, 0x8037},      {0x6061, 0, 1, 1},      {0x6040, 0, 2, 0},
        {0x2202, 0, 2, 0},           {0x607d, 0, 1, 2},      {0x607d, 1, 4, 0x80000000u},
        {0x607d, 2, 4, 0x7fffffffu}, {0x6081, 0, 4, 60},     {0x6083, 0, 4, 100},
        {0x6084, 0, 4, 100},         {0x2400, 0, 2, 16},     {0x2400, 1, 2, 0xa500},
        {0x2400, 2, 2, 1},           {0x2400, 3, 2, 2},      {0x2400, 4, 2, 3},
        {0x2400, 5, 2, 0},           {0x2400, 6, 2, 0},      {0x2400, 7, 2, 0},
        {0x2400, 8, 2, 0},           {0x2500, 0, 2, 2},      {0x2501, 0, 2, 4000},
        {0x1000, 0, 4, 0x00040192},  {0x1009, 0, 2, 0x0c},   {0x100a, 0, 2, 0x20},
        {0x1018, 0, 1, 3},           {0x1018, 1, 4, 0x1000}, {0x1018, 2, 4, 1},
        {0x1018, 3, 4, 0},           {0x2000, 0, 2, 0x0400}, {0x2001, 0, 2, 0x0200}};
    unsigned downloads = 0, uploads = 0;
    uint16_t lostIndex = 0;
    uint8_t lostSubindex = 0;
    bool lostDownload = false;
    Entry& find(uint16_t index, uint8_t subindex = 0) {
        for (Entry& e : values)
            if (e.index == index && e.subindex == subindex)
                return e;
        CHECK(false);
        return values[0];
    }
    bool answer(const M::Frame& request, M::Frame& reply) {
        if (request.data[0] == 0x80)
            return false; // An actual client abort is cleanup, never another dictionary request.
        CHECK(request.id == 0x601 && request.payloadLength == 8);
        const uint16_t index = uint16_t(request.data[1] | (uint16_t(request.data[2]) << 8));
        Entry& e = find(index, request.data[3]);
        CHECK(e.width >= 1 && e.width <= 4);
        const bool download = request.data[0] != 0x40;
        if (download) {
            ++downloads;
            CHECK(request.data[0] == (e.width == 2 ? 0x2b : 0x23));
            e.raw = 0;
            for (unsigned i = 0; i < 4 && i < e.width; ++i)
                e.raw |= uint32_t(request.data[4 + i]) << (8 * i);
        } else
            ++uploads;
        if (lostIndex == index && lostSubindex == e.subindex && lostDownload == download)
            return false; // The request arrived; a download's effect survives its lost reply.
        reply = OwnerRig::frame(0x581, 8);
        reply.data[0] = download ? 0x60 : e.width == 1 ? 0x4f : e.width == 2 ? 0x4b : 0x43;
        reply.data[1] = request.data[1];
        reply.data[2] = request.data[2];
        reply.data[3] = request.data[3];
        if (!download)
            for (unsigned i = 0; i < 4 && i < e.width; ++i)
                reply.data[4 + i] = uint8_t(e.raw >> (8 * i));
        return true;
    }
};
struct Wire : OwnerRig::WireBase {
    E::ProbeSettings* settings = nullptr;
    E::AxisSession* session = nullptr;
    unsigned settingsGuards = 0;
    static bool guard(void* context, O::SequenceId sequence, const C::SdoTransfer& work,
                      uint64_t now) {
        Wire& wire = static_cast<Wire&>(*static_cast<OwnerRig::WireBase*>(context));
        if (wire.settings && wire.settings->retained()) {
            ++wire.settingsGuards;
            return wire.settings->preflight(sequence, work, now);
        }
        return wire.session && wire.session->ownsDownload(sequence, work);
    }
    O::Transport transport() {
        O::Transport value = plumbing();
        value.beforeTransmit = guard;
        return value;
    }
};
struct Rig {
    // Experiment grants below belong exclusively to this HOST_FAKE fixture. They are never
    // installed in ProbeSettings and grant no production CLI or device read permission.
    enum class Environment { HOST_FAKE };
    const Environment environment = Environment::HOST_FAKE;
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::AxisSession session;
    L::ReadTarget current;
    E::ProbeSettings settings;
    Dictionary drive;
    uint64_t now = 0;
    uint32_t authorization = 0, grantedOperation = 0;
    uint8_t grantedStep = 0;
    unsigned served = 0;
    Rig()
        : owner(services, wire.transport(), 141), session(owner),
          settings(owner, services, session, current) {
        current.channel = OwnerRig::route();
        current.nodeGeneration = 11;
        CHECK(services.configure(0, current.channel, true, true));
        C::NetworkRoute network;
        network.binding = current.channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        M::Frame heartbeat = OwnerRig::frame(0x701, 1);
        heartbeat.data[0] = 127;
        wire.push(heartbeat, 0);
        CHECK(owner.service(0, OwnerRig::budget()));
        CHECK(services.configurationEligible(0, current.channel.binding, 0));
        CHECK(session.open(current, L::SettingsModel::CL86_C, 1, 0));
        wire.session = &session;
        wire.settings = &settings;
    }
    void grant(const C::SdoTransfer& work) {
        if (work.direction != C::SdoDirection::UPLOAD ||
            work.readEffect != C::SdoReadEffect::UNQUALIFIED)
            return;
        if (work.operation == grantedOperation && work.step == grantedStep)
            return;
        CHECK(environment == Environment::HOST_FAKE);
        CHECK(services.authorizeUploadExperiment(0, work, current.nodeGeneration, ++authorization,
                                                 now));
        grantedOperation = work.operation;
        grantedStep = work.step;
    }
    void turn() {
        ++now;
        CHECK(owner.service(now, OwnerRig::budget()));
        for (; served < wire.sentCount; ++served) {
            M::Frame reply;
            if (drive.answer(wire.sent[served], reply))
                wire.push(reply, now + 1);
        }
        session.service(now);
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work) {
        grant(work);
        CHECK(session.admitPrepared(work, now) == O::BusAdmission::ACCEPTED);
        C::SdoCompletion completion;
        for (unsigned i = 0; i < 200; ++i) {
            turn();
            if (session.settle(completion))
                return completion;
        }
        CHECK(false);
        return completion;
    }
    void read(L::DriverContext& output, L::DriverGroup group) {
        const uint32_t operation = session.beginOperation(DEADLINE, now);
        CHECK(operation &&
              L::prepareDriverRead(output, current, operation, now, DEADLINE, options(), group));
        while (output.read.state == M::ReadState::ACTIVE) {
            L::PreparedRead next;
            CHECK(L::nextDriver(output, now, next) && next.ready);
            const C::SdoCompletion completion = exchange(next.transfer);
            CHECK(L::advanceDriver(output, completion, now));
        }
        CHECK(output.read.state == M::ReadState::SUCCEEDED);
    }
    void readState(L::ReadContext& output) {
        const uint32_t operation = session.beginOperation(DEADLINE, now);
        CHECK(operation &&
              L::prepareState(output, current, operation, now, DEADLINE, options(), STATE));
        while (output.state == M::ReadState::ACTIVE) {
            L::PreparedRead next;
            CHECK(L::nextRead(output, now, next) && next.ready);
            const C::SdoCompletion completion = exchange(next.transfer);
            CHECK(L::advanceRead(output, completion, now));
        }
        CHECK(output.state == M::ReadState::SUCCEEDED);
    }
    void readIdentity(L::ReadContext& output) {
        const uint32_t operation = session.beginOperation(DEADLINE, now);
        CHECK(operation &&
              L::prepareIdentity(output, current, operation, now, DEADLINE, options()));
        while (output.state == M::ReadState::ACTIVE) {
            L::PreparedRead next;
            CHECK(L::nextRead(output, now, next) && next.ready);
            const C::SdoCompletion completion = exchange(next.transfer);
            CHECK(L::advanceRead(output, completion, now));
        }
        CHECK(output.state == M::ReadState::SUCCEEDED);
    }
    L::AxisReservation reservation() {
        const O::SequenceId id = session.sequence();
        const O::Sequence* held = owner.sequence(id);
        CHECK(held && M::sameBinding(held->binding, current.channel.binding));
        L::AxisReservation result;
        result.target = current;
        result.model = L::SettingsModel::CL86_C;
        result.owner = owner.identity();
        result.sequence = uint32_t(id.slot) + 1;
        result.generation = id.generation;
        result.acquiredUs = held->acquiredUs;
        return result;
    }
    C::SdoCompletion acknowledgeControl() {
        C::SdoTransfer work;
        work.channel = current.channel;
        work.operation = work.request = session.beginOperation(DEADLINE, now);
        CHECK(work.operation);
        work.step = 1;
        work.index = 0x6040;
        work.direction = C::SdoDirection::DOWNLOAD;
        work.size = 2;
        work.data[0] = 0x0f;
        work.deadlineUs = DEADLINE;
        work.queueTimeoutUs = work.responseTimeoutUs = 100;
        const C::SdoCompletion result = exchange(work);
        CHECK(result.outcome == C::SdoOutcome::SUCCESS && result.protocolAcknowledged);
        CHECK(session.latestControl());
        return result;
    }
    void prerequisites(L::DriverContext& prior, L::ReadContext& state, L::DriverGroup group,
                       const C::SdoCompletion* control = nullptr) {
        read(prior, group);
        readState(state);
        CHECK(settings.setPrerequisites(prior, state, control, L::SettingsModel::CL86_C, options(),
                                        AGE));
    }
    void grantReadback() {
        // The bridge may already have queued the next typed readback. Authorize that exact
        // copied transfer before the ordinary owner performs native service admission.
        const L::DriverSettingsContext& context = settings.context();
        if (context.state == L::SettingsState::READING_BACK) {
            L::PreparedRead next;
            CHECK(L::nextDriver(context, context.reservation, current.channel.binding, now, next));
            if (next.ready)
                grant(next.transfer);
        }
    }
    void progress() {
        grantReadback();
        turn();
        const M::Status status = settings.service(now);
        CHECK(status || settings.context().state == L::SettingsState::FAILED);
    }
    void finish() {
        for (unsigned i = 0; i < 1000 && settings.active(); ++i)
            progress();
        CHECK(!settings.active());
    }
};
struct ConsoleFixture {
    struct Bytes {
        char output[8192] = {};
        size_t used = 0;
        uint64_t now = 0;
        static bool write(void* context, uint8_t byte) {
            Bytes& self = *static_cast<Bytes*>(context);
            CHECK(self.used + 1 < sizeof self.output);
            self.output[self.used++] = static_cast<char>(byte);
            self.output[self.used] = 0;
            return true;
        }
        static bool clock(void* context, uint64_t& output) {
            output = static_cast<Bytes*>(context)->now;
            return true;
        }
        E::ProbeCallbacks callbacks() {
            E::ProbeCallbacks value;
            value.context = this;
            value.writeByte = write;
            value.nowUs = clock;
            return value;
        }
    } bytes;
    Rig rig;
    E::ProbePlatform platform;
    E::ProbeApp app;
    E::ProbeConsole console;
    ConsoleFixture()
        : platform(bytes.callbacks()), app(rig.owner, platform), console(app, rig.owner, platform) {
        CHECK(app.select(rig.current));
        console.attachSettings(rig.settings);
    }
    void command(const char* text) {
        for (const char* p = text; *p; ++p)
            console.feed(static_cast<uint8_t>(*p), rig.now);
        console.feed('\n', rig.now);
    }
    void turn() {
        rig.grantReadback();
        rig.turn(); // Sole CAN service and session progression, independent of console output.
        bytes.now = rig.now;
        platform.service();
        console.service(rig.now); // The production console owns bridge progression in this path.
    }
    void drain() {
        for (unsigned i = 0; i < 200; ++i)
            turn();
        CHECK(console.drained() && platform.stats().outputUsed == 0);
        CHECK(console.stats().outputDropped == 0);
        const char* begin = bytes.output;
        while (*begin) {
            const char* end = std::strchr(begin, '\n');
            CHECK(end && size_t(end - begin + 1) < E::ProbeConsole::RECORD_CAPACITY);
            CHECK(*begin == '{' && end[-1] == '}');
            begin = end + 1;
        }
    }
    void record(uint32_t id, const char* field) {
        char identity[40];
        std::snprintf(identity, sizeof identity, "\"id\":%lu,", static_cast<unsigned long>(id));
        const char* begin = std::strstr(bytes.output, identity);
        CHECK(begin);
        const char* end = std::strchr(begin, '\n');
        const char* found = std::strstr(begin, field);
        if (!found || !end || found >= end)
            std::fprintf(stderr, "Missing [%s] for %s in:\n%s", field, identity, bytes.output);
        CHECK(found && end && found < end);
    }
    void terminal(uint32_t id, const char* field) {
        char identity[40];
        std::snprintf(identity, sizeof identity, "\"id\":%lu,", static_cast<unsigned long>(id));
        for (const char* begin = bytes.output; *begin;) {
            const char* end = std::strchr(begin, '\n');
            CHECK(end);
            const char* type = std::strstr(begin, "\"type\":\"terminal\"");
            const char* request = std::strstr(begin, identity);
            if (type && type < end && request && request < end) {
                const char* found = std::strstr(begin, field);
                CHECK(found && found < end);
                return;
            }
            begin = end + 1;
        }
        std::fprintf(stderr, "Missing terminal %s in:\n%s", identity, bytes.output);
        CHECK(false);
    }
};
L::SoftLimitSettings limits() {
    L::SoftLimitSettings value;
    value.enable = L::SoftLimitEnable::AFTER_HOMING;
    return value;
}
L::IoSettings inputs() {
    L::IoSettings value;
    value.invertedInputs = 1;
    for (unsigned i = 0; i < 7; ++i)
        value.functions[i] = L::InputFunction::UNDEFINED;
    return value;
}
void invalidWholeCandidatesDoNotStartWork() {
    Rig rig;
    CHECK(!rig.settings.requestIo(inputs(), rig.now, DEADLINE));
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::IO_INPUT_CONFIGURATION);
    const uint32_t operation = rig.session.nextOperation();
    CHECK(!rig.settings.setPrerequisites(prior, state, nullptr, L::SettingsModel::OL86_C, options(),
                                         AGE));
    CHECK(!rig.settings.retained() && rig.session.nextOperation() == operation);
    const unsigned sent = rig.wire.sentCount;
    L::IoSettings bad = inputs();
    bad.functions[6] = L::InputFunction::UNKNOWN; // Last field invalid; no valid prefix may escape.
    CHECK(!rig.settings.requestIo(bad, rig.now, DEADLINE));
    CHECK(!rig.settings.retained() && rig.session.nextOperation() == operation);
    CHECK(!rig.session.busy() && rig.wire.settingsGuards == 0);
    rig.turn();
    CHECK(rig.wire.sentCount == sent && rig.drive.downloads == 0);
    L::ReadContext stale = state;
    ++stale.target.channel.binding.configurationGeneration;
    CHECK(rig.settings.setPrerequisites(prior, stale, nullptr, L::SettingsModel::CL86_C, options(),
                                        AGE));
    CHECK(!rig.settings.requestIo(inputs(), rig.now, DEADLINE));
    CHECK(!rig.settings.retained() && rig.session.nextOperation() == operation);
}
void explicitSelectorRoundTripAndBindingFollow() {
    Rig rig;
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::SOFT_LIMITS);
    const M::Binding old = rig.current.channel.binding;
    const unsigned downloads = rig.drive.downloads, uploads = rig.drive.uploads;
    CHECK(rig.settings.requestSoftLimits(limits(), rig.now, DEADLINE));
    CHECK(rig.settings.retained() && rig.wire.settingsGuards == 0);
    CHECK(rig.settings.service(rig.now)); // Queue the real prepared write, without TX.
    L::PreparedRead expected;
    CHECK(L::nextDriver(rig.settings.context(), rig.settings.context().reservation,
                        rig.current.channel.binding, rig.now, expected) &&
          expected.ready);
    const Saved<L::DriverSettingsContext> retained(rig.settings.context());
    C::SdoTransfer wrong = expected.transfer;
    wrong.data[0] ^= 1;
    CHECK(!rig.settings.preflight(rig.session.sequence(), wrong, rig.now));
    O::SequenceId foreign = rig.session.sequence();
    ++foreign.generation;
    CHECK(!rig.settings.preflight(foreign, expected.transfer, rig.now));
    retained.unchanged(rig.settings.context());
    CHECK(M::sameBinding(old, rig.current.channel.binding));
    rig.finish();
    const L::DriverSettingsContext& result = rig.settings.context();
    CHECK(result.state == L::SettingsState::SUCCEEDED && result.confirmed == 1);
    CHECK(rig.wire.settingsGuards == 1 && rig.drive.downloads == downloads + 1);
    CHECK(rig.drive.uploads == uploads + 4 && rig.drive.find(0x2202).raw == 1);
    CHECK(rig.current.channel.binding.configurationGeneration == old.configurationGeneration + 1);
    CHECK(rig.owner.matchesChannel(rig.current.channel, rig.current.nodeGeneration));
    CHECK(rig.owner.sequence(rig.session.sequence())); // The original exclusion still holds.
    L::DriverObservation observed;
    CHECK(L::getDriver(result, result.reservation, rig.current, rig.now, AGE, observed));
    CHECK(observed.softLimitSelectorRaw == 1 && observed.minimumNative == INT32_MIN &&
          observed.maximumNative == INT32_MAX);
    CHECK(!M::sameBinding(rig.session.binding(), rig.current.channel.binding));
    CHECK(rig.settings.finishBinding(rig.now));
    CHECK(M::sameBinding(rig.session.binding(), rig.current.channel.binding));
    CHECK(!rig.session.latestControl());
    const unsigned sent = rig.wire.sentCount;
    CHECK(rig.settings.service(rig.now) && rig.settings.finishBinding(rig.now));
    CHECK(rig.settings.release() && !rig.settings.release());
    rig.turn();
    CHECK(rig.wire.sentCount == sent);
    CHECK(!rig.settings.requestSoftLimits(limits(), rig.now, DEADLINE)); // Borrow ended.
}
void partialAcknowledgementRetainsUnknownEffect() {
    Rig rig;
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::IO_INPUT_CONFIGURATION);
    rig.drive.lostIndex = 0x2400;
    rig.drive.lostSubindex = 2;
    rig.drive.lostDownload = true;
    const unsigned downloads = rig.drive.downloads;
    const M::Generation generation = rig.current.channel.binding.configurationGeneration;
    CHECK(rig.settings.requestIo(inputs(), rig.now, DEADLINE));
    rig.finish();
    const L::DriverSettingsContext& result = rig.settings.context();
    CHECK(result.state == L::SettingsState::FAILED &&
          result.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
    CHECK(result.confirmed == 2 && result.effectsPossible && result.uncertain);
    CHECK(result.evidence.transfer.index == 0x2400 && result.evidence.transfer.subindex == 2);
    CHECK(result.evidence.transmissionPossible && !result.evidence.protocolAcknowledged);
    CHECK(rig.drive.downloads == downloads + 2 && rig.wire.settingsGuards == 2);
    CHECK(rig.drive.find(0x2400, 1).raw == 0xa501 && rig.drive.find(0x2400, 2).raw == 0);
    CHECK(rig.drive.find(0x2400, 3).raw == 2); // No third write after the lost reply.
    CHECK(rig.current.channel.binding.configurationGeneration == generation + 1);
    CHECK(!rig.settings.finishBinding(rig.now)); // The unresolved old route cannot migrate.
    const Saved<L::DriverSettingsContext> saved(result);
    for (unsigned i = 0; i < 10; ++i)
        CHECK(rig.settings.service(rig.now));
    saved.unchanged(rig.settings.context());
    CHECK(rig.drive.downloads == downloads + 2);
    CHECK(!rig.settings.requestIo(inputs(), rig.now, DEADLINE));
    const M::Status migration = rig.settings.bindingStatus();
    CHECK(rig.settings.release()); // Explicit history discard cannot repair the route.
    CHECK(rig.settings.bindingStatus().code == migration.code &&
          rig.settings.bindingStatus().detail == migration.detail);
    CHECK(rig.settings.setPrerequisites(prior, state, nullptr, L::SettingsModel::CL86_C, options(),
                                        AGE));
    CHECK(!rig.settings.requestIo(inputs(), rig.now, DEADLINE));
    CHECK(rig.drive.downloads == downloads + 2);
}
void acknowledgementDoesNotHideReadbackFailure() {
    Rig rig;
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::SOFT_LIMITS);
    rig.drive.lostIndex = 0x2202;
    rig.drive.lostDownload = false;
    CHECK(rig.settings.requestSoftLimits(limits(), rig.now, DEADLINE));
    rig.finish();
    const L::DriverSettingsContext& result = rig.settings.context();
    CHECK(result.state == L::SettingsState::FAILED && result.confirmed == 1);
    CHECK(result.outcome == M::ReadOutcome::SERVICE_TIMEOUT && result.uncertain);
    CHECK(result.evidence.transfer.direction == C::SdoDirection::UPLOAD);
    CHECK(rig.drive.downloads == 1 && rig.drive.find(0x2202).raw == 1);
    CHECK(rig.settings.lastCompletion() &&
          rig.settings.lastCompletion()->outcome == C::SdoOutcome::TIMEOUT);
    const unsigned sent = rig.wire.sentCount;
    for (unsigned i = 0; i < 5; ++i)
        CHECK(rig.settings.service(rig.now));
    CHECK(rig.wire.sentCount == sent && rig.drive.downloads == 1);
}
void driverRequiresActualControlAndCompleteCandidate() {
    Rig rig;
    const C::SdoCompletion actual = rig.acknowledgeControl();
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::MOTION_PARAMETERS, &actual);
    L::DriverSettings candidate;
    candidate.positionSpeedRpm = 120;
    candidate.rampUpTimeMs = 200;
    candidate.rampDownTimeMs = 300;
    const uint32_t operation = rig.session.nextOperation();
    const unsigned sent = rig.wire.sentCount;
    C::SdoCompletion fabricated = actual;
    ++fabricated.completedUs;
    CHECK(rig.settings.setPrerequisites(prior, state, &fabricated, L::SettingsModel::CL86_C,
                                        options(), AGE));
    CHECK(!rig.settings.requestDriver(candidate, rig.now, DEADLINE));
    CHECK(!rig.settings.retained() && rig.session.nextOperation() == operation);
    CHECK(rig.settings.setPrerequisites(prior, state, &actual, L::SettingsModel::CL86_C, options(),
                                        AGE));
    L::DriverSettings invalid = candidate;
    invalid.rampDownTimeMs = 2001;
    CHECK(!rig.settings.requestDriver(invalid, rig.now, DEADLINE));
    CHECK(rig.session.nextOperation() == operation && rig.wire.sentCount == sent);
    CHECK(rig.settings.requestDriver(candidate, rig.now, DEADLINE));
    rig.finish();
    const L::DriverSettingsContext& result = rig.settings.context();
    CHECK(result.state == L::SettingsState::SUCCEEDED && result.confirmed == 7);
    CHECK(rig.drive.downloads == 4); // The real prerequisite 000F plus three typed writes.
    CHECK(rig.drive.find(0x6081).raw == 120 && rig.drive.find(0x6083).raw == 200 &&
          rig.drive.find(0x6084).raw == 300);
    CHECK(rig.current.channel.binding.configurationGeneration ==
          prior.read.target.channel.binding.configurationGeneration + 1);
    L::DriverObservation observed;
    CHECK(rig.owner.sequence(rig.session.sequence()));
    CHECK(L::getDriver(result, result.reservation, rig.current, rig.now, AGE, observed));
    CHECK(observed.positionSpeedRpm == 120 && observed.rampUpTimeMs == 200 &&
          observed.rampDownTimeMs == 300);
    CHECK(rig.settings.finishBinding(rig.now) && rig.settings.release());
}
void withdrawalBeforeTransmissionChangesNoConfiguration() {
    Rig rig;
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::SOFT_LIMITS);
    const M::Binding before = rig.current.channel.binding;
    CHECK(rig.settings.requestSoftLimits(limits(), rig.now, DEADLINE));
    CHECK(rig.settings.service(rig.now)); // Admitted locally, before any owner TX turn.
    CHECK(!rig.settings.cancel(rig.now - 1));
    CHECK(rig.settings.cancel(rig.now));
    rig.finish();
    CHECK(rig.settings.context().state == L::SettingsState::FAILED);
    CHECK(rig.settings.context().outcome == M::ReadOutcome::CANCELLED);
    CHECK(!rig.settings.context().effectsPossible && !rig.settings.context().uncertain);
    CHECK(rig.settings.lastCompletion() && !rig.settings.lastCompletion()->transmissionPossible);
    CHECK(M::sameBinding(before, rig.current.channel.binding) && rig.drive.downloads == 0);
    CHECK(rig.settings.release());
}
void loopSelectorUsesTypedReadbackWithoutEncoderWrite() {
    Rig rig;
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::CONTROL);
    const uint32_t operation = rig.session.nextOperation();
    L::ControlSettings invalid;
    CHECK(!rig.settings.requestControl(invalid, rig.now, DEADLINE));
    CHECK(!rig.settings.retained() && rig.session.nextOperation() == operation);
    L::ControlSettings candidate;
    candidate.loopMode = L::LoopMode::OPEN_LOOP;
    CHECK(rig.settings.requestControl(candidate, rig.now, DEADLINE));
    rig.finish();
    const L::DriverSettingsContext& result = rig.settings.context();
    CHECK(result.state == L::SettingsState::SUCCEEDED && result.confirmed == 1);
    CHECK(rig.drive.downloads == 1 && rig.drive.find(0x2500).raw == 1);
    CHECK(rig.drive.find(0x2501).raw == 4000 && rig.wire.settingsGuards == 1);
    L::ControlObservation observed;
    CHECK(rig.owner.sequence(rig.session.sequence()));
    CHECK(L::getControl(result, result.reservation, rig.current, rig.now, AGE, observed));
    CHECK(observed.loopMode == L::LoopMode::OPEN_LOOP && observed.encoderResolutionRaw == 4000);
    CHECK(rig.settings.finishBinding(rig.now) && rig.settings.release());
}
void recordedReapplicationRequiresExactHelperPreparation() {
    Rig rig;
    L::ReadContext identity;
    L::DriverContext prior;
    rig.readIdentity(identity);
    rig.read(prior, L::DriverGroup::MOTION_PARAMETERS);
    const L::AxisReservation originalReservation = rig.reservation();
    E::ConfigurationRecord original;
    CHECK(E::captureConfiguration(original, originalReservation, rig.current, identity, prior, 1,
                                  10, 20, rig.now, AGE));
    const uint32_t originalGeneration = rig.current.channel.binding.configurationGeneration;

    // Change the live registers through the actual typed path; the retained record keeps
    // its older observation rather than pretending it was acquired at the new generation.
    C::SdoCompletion control = rig.acknowledgeControl();
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::MOTION_PARAMETERS, &control);
    L::DriverSettings changed;
    changed.positionSpeedRpm = 120;
    changed.rampUpTimeMs = 200;
    changed.rampDownTimeMs = 300;
    CHECK(rig.settings.requestDriver(changed, rig.now, DEADLINE));
    rig.finish();
    CHECK(rig.settings.finishBinding(rig.now) && rig.settings.release());

    control = rig.acknowledgeControl();
    rig.readIdentity(identity);
    rig.prerequisites(prior, state, L::DriverGroup::MOTION_PARAMETERS, &control);
    const L::AxisReservation live = rig.reservation();
    E::ConfigurationRestoreReview review;
    review.target = rig.current;
    review.fixtureRecord = 10;
    review.firmwareRecord = 20;
    review.routeRecord = 30;
    E::ConfigurationRestoreContext prepared;
    const uint32_t operation = rig.session.nextOperation();
    CHECK(E::prepareConfigurationRestore(prepared, original, review, live, rig.current, identity,
                                         prior, state, control, operation, rig.now, DEADLINE,
                                         options(), AGE));
    const Saved<E::ConfigurationRestoreContext> immutable(prepared);
    const unsigned sent = rig.wire.sentCount;
    for (unsigned field = 0; field < 4; ++field) {
        E::ConfigurationRestoreContext wrong = prepared;
        if (field == 0)
            ++wrong.settings.reservation.generation;
        else if (field == 1)
            ++wrong.settings.operation;
        else if (field == 2)
            --wrong.settings.startedUs;
        else
            --wrong.settings.deadlineUs;
        CHECK(!rig.settings.requestRestore(wrong, rig.now, DEADLINE));
        CHECK(!rig.settings.retained() && rig.session.nextOperation() == operation);
        CHECK(rig.wire.sentCount == sent);
    }
    immutable.unchanged(prepared);
    CHECK(rig.settings.requestRestore(prepared, rig.now, DEADLINE));
    rig.finish();
    CHECK(rig.settings.context().state == L::SettingsState::SUCCEEDED);
    CHECK(rig.settings.context().confirmed == 7 && rig.drive.downloads == 8);
    CHECK(rig.drive.find(0x6081).raw == 60 && rig.drive.find(0x6083).raw == 100 &&
          rig.drive.find(0x6084).raw == 100);
    CHECK(rig.current.channel.binding.configurationGeneration == originalGeneration + 2);
    CHECK(original.configuration.target.channel.binding.configurationGeneration ==
          originalGeneration);
    immutable.unchanged(prepared);
    CHECK(rig.settings.finishBinding(rig.now) && rig.settings.release());
}
void consoleSettingsRejectBeforeAdmissionAndRetainMatchedResult() {
    ConsoleFixture fixture;
    Rig& rig = fixture.rig;
    const unsigned initialSent = rig.wire.sentCount;
    const uint32_t initialOperation = rig.session.nextOperation();
    fixture.command("@401 driver limits 1");
    fixture.drain();
    fixture.record(401, "\"ok\":false");
    CHECK(!rig.settings.retained() && rig.session.nextOperation() == initialOperation);
    CHECK(rig.wire.sentCount == initialSent && rig.drive.downloads == 0);

    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::SOFT_LIMITS);
    const unsigned sent = rig.wire.sentCount;
    const uint32_t operation = rig.session.nextOperation();
    fixture.command("@402 driver limits 2");
    fixture.drain();
    fixture.record(402, "native_settings_range");
    CHECK(!rig.settings.retained() && rig.session.nextOperation() == operation);
    CHECK(rig.wire.sentCount == sent);

    fixture.command("@403 driver limits 1");
    fixture.drain();
    fixture.record(403, "\"result\":\"accepted\",\"operation_id\":1");
    fixture.terminal(403, "\"outcome\":\"SETTINGS_READBACK_MATCHED\"");
    fixture.terminal(403, "\"detail\":1");
    fixture.terminal(403, "\"command_id\":403,\"operation_id\":1");
    CHECK(rig.settings.context().state == L::SettingsState::SUCCEEDED);
    CHECK(rig.settings.context().confirmed == 1 && rig.drive.downloads == 1);
    CHECK(rig.drive.find(0x2202).raw == 1 && rig.wire.settingsGuards == 1);
    CHECK(rig.settings.context().readback.read.state == M::ReadState::SUCCEEDED);

    fixture.command("@404 result 1");
    fixture.drain();
    fixture.record(404, "\"outcome\":\"SETTINGS_READBACK_MATCHED\"");
    fixture.record(404, "\"command_id\":403,\"operation_id\":1");
    CHECK(rig.settings.retained());
    CHECK(rig.settings.finishBinding(rig.now)); // Explicit application reconciliation, no traffic.
    fixture.command("@405 release 1");
    fixture.drain();
    fixture.record(405, "\"ok\":true");
    CHECK(!rig.settings.retained() && rig.drive.downloads == 1);
    fixture.command("@406 result 1");
    fixture.drain();
    fixture.record(406, "unknown_operation");
}
void consolePartialWriteKeepsUncertaintyThroughLookupAndRelease() {
    ConsoleFixture fixture;
    Rig& rig = fixture.rig;
    L::DriverContext prior;
    L::ReadContext state;
    rig.prerequisites(prior, state, L::DriverGroup::IO_INPUT_CONFIGURATION);
    rig.drive.lostIndex = 0x2400;
    rig.drive.lostSubindex = 2;
    rig.drive.lostDownload = true;
    fixture.command("@501 io set 1 0 0 0 0 0 0 0");
    fixture.drain();
    fixture.record(501, "\"result\":\"accepted\",\"operation_id\":1");
    fixture.terminal(501, "\"outcome\":\"SETTINGS_FAILED\"");
    fixture.terminal(501, "\"uncertain\":true");
    fixture.terminal(501, "\"detail\":2");
    CHECK(rig.settings.context().outcome == M::ReadOutcome::SERVICE_TIMEOUT);
    CHECK(rig.settings.context().confirmed == 2 && rig.drive.downloads == 2);
    const unsigned sent = rig.wire.sentCount;
    fixture.command("@502 result 1");
    fixture.drain();
    fixture.record(502, "\"outcome\":\"SETTINGS_FAILED\"");
    fixture.record(502, "\"uncertain\":true");
    fixture.record(502, "\"command_id\":501,\"operation_id\":1");
    CHECK(rig.wire.sentCount == sent && rig.settings.retained());
    fixture.command("@503 release 1");
    fixture.drain();
    fixture.record(503, "\"ok\":true");
    CHECK(!rig.settings.retained() && rig.wire.sentCount == sent);
    CHECK(!rig.owner.matchesChannel(rig.current.channel, rig.current.nodeGeneration));
    CHECK(rig.drive.find(0x2400, 3).raw == 2 && rig.drive.downloads == 2);
}
} // namespace
int main() {
    invalidWholeCandidatesDoNotStartWork();
    explicitSelectorRoundTripAndBindingFollow();
    partialAcknowledgementRetainsUnknownEffect();
    acknowledgementDoesNotHideReadbackFailure();
    driverRequiresActualControlAndCompleteCandidate();
    withdrawalBeforeTransmissionChangesNoConfiguration();
    loopSelectorUsesTypedReadbackWithoutEncoderWrite();
    recordedReapplicationRequiresExactHelperPreparation();
    consoleSettingsRejectBeforeAdmissionAndRetainMatchedResult();
    consolePartialWriteKeepsUncertaintyThroughLookupAndRelease();
    std::printf("probe_settings_app: %u checks passed (HOST_FAKE only)\n", checks);
}
