// SPDX-License-Identifier: MIT
// HOST_FAKE only. Actual typed preparation, owner dispatch and native terminals exercise
// the application bridge; the scalar dictionary supplies synthetic wire traffic only.
#include "ProbeCommissioning.h"
#include "ProbeConsole.h"
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
    L::ReadOptions result;
    result.queueTimeoutUs = result.responseTimeoutUs = 100;
    return result;
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
struct Dictionary {
    Entry entries[28] = {
        {0x1000, 0, 4, 0x00040192}, {0x1009, 0, 2, 0x0c},       {0x100a, 0, 2, 0x20},
        {0x1018, 0, 1, 3},          {0x1018, 1, 4, 0x1000},     {0x1018, 2, 4, 1},
        {0x1018, 3, 4, 0},          {0x2000, 0, 2, 0x400},      {0x2001, 0, 2, 0x200},
        {0x2100, 0, 2, 1},          {0x2101, 0, 2, 0},          {0x2102, 0, 2, 0},
        {0x6041, 0, 2, 0x8037},     {0x6061, 0, 1, 1},          {0x6081, 0, 4, 60},
        {0x6083, 0, 4, 100},        {0x6084, 0, 4, 100},        {0x1017, 0, 2, 0},
        {0x2300, 0, 2, 0},          {0x1800, 1, 4, 0x181},      {0x1800, 2, 1, 255},
        {0x1800, 3, 2, 100},        {0x1800, 5, 2, 0},          {0x1a00, 0, 1, 3},
        {0x1a00, 1, 4, 0x60410010}, {0x1a00, 2, 4, 0x60610008}, {0x1a00, 3, 4, 0x60640020},
        {0x1a00, 4, 4, 0}};
    enum class Fault { NONE, LOST, ABORT };
    Fault fault = Fault::NONE;
    uint16_t faultIndex = 0;
    uint8_t faultSubindex = 0;
    bool faultDownload = true, wrongReplyRoute = false;
    unsigned uploads = 0, downloads = 0;
    Entry& find(uint16_t index, uint8_t subindex = 0) {
        for (Entry& entry : entries)
            if (entry.index == index && entry.subindex == subindex)
                return entry;
        CHECK(false);
        return entries[0];
    }
    bool answer(const M::Frame& request, M::Frame& reply) {
        if (request.data[0] == 0x80)
            return false;
        CHECK(request.id == 0x601 && request.payloadLength == 8);
        const uint16_t index = uint16_t(request.data[1] | (uint16_t(request.data[2]) << 8));
        Entry& entry = find(index, request.data[3]);
        CHECK(entry.width == 1 || entry.width == 2 || entry.width == 4);
        const bool download = request.data[0] != 0x40;
        const bool selected = fault != Fault::NONE && index == faultIndex &&
                              entry.subindex == faultSubindex && download == faultDownload;
        reply = OwnerRig::frame(wrongReplyRoute ? 0x582 : 0x581, 8);
        reply.data[1] = request.data[1];
        reply.data[2] = request.data[2];
        reply.data[3] = request.data[3];
        (download ? downloads : uploads)++;
        if (selected && fault == Fault::ABORT) {
            reply.data[0] = 0x80;
            const uint32_t abort = 0xf1234567;
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = uint8_t(abort >> (8 * i));
            return true;
        }
        if (download) {
            CHECK(request.data[0] == (entry.width == 1 ? 0x2f : entry.width == 2 ? 0x2b : 0x23));
            entry.raw = 0;
            for (unsigned i = 0; i < 4 && i < entry.width; ++i)
                entry.raw |= uint32_t(request.data[4 + i]) << (8 * i);
        }
        if (selected && fault == Fault::LOST)
            return false;
        reply.data[0] = download ? 0x60 : entry.width == 1 ? 0x4f : entry.width == 2 ? 0x4b : 0x43;
        if (!download)
            for (unsigned i = 0; i < 4 && i < entry.width; ++i)
                reply.data[4 + i] = uint8_t(entry.raw >> (8 * i));
        return true;
    }
};
struct Wire : OwnerRig::WireBase {
    E::ProbeCommissioning* commissioning = nullptr;
    unsigned guards = 0;
    static bool guard(void* context, O::SequenceId sequence, const C::SdoTransfer& transfer,
                      uint64_t now) {
        Wire& wire = static_cast<Wire&>(*static_cast<OwnerRig::WireBase*>(context));
        ++wire.guards;
        return wire.commissioning && wire.commissioning->preflight(sequence, transfer, now);
    }
    O::Transport transport() {
        O::Transport result = plumbing();
        result.beforeTransmit = guard;
        return result;
    }
};
struct Rig {
    enum class Environment { HOST_FAKE };
    const Environment environment = Environment::HOST_FAKE;
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::AxisSession session;
    L::ReadTarget current;
    E::ProbeCommissioning commissioning;
    Dictionary drive;
    uint64_t now = 10;
    uint32_t authorization = 0, grantOperation = 0, grantStep = 0;
    unsigned served = 0;
    Rig()
        : owner(services, wire.transport(), 151), session(owner),
          commissioning(owner, services, session, current) {
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
        wire.push(heartbeat, now); // The actual observation is not older than the reservation.
        CHECK(owner.service(now, OwnerRig::budget()));
        CHECK(services.configurationEligible(0, current.channel.binding, now));
        CHECK(session.open(current, L::SettingsModel::CL86_C, 1, now));
        wire.commissioning = &commissioning;
    }
    L::AxisReservation reservation() {
        const O::SequenceId id = session.sequence();
        const O::Sequence* sequence = owner.sequence(id);
        CHECK(sequence && M::sameBinding(sequence->binding, current.channel.binding));
        L::AxisReservation result;
        result.target = current;
        result.model = L::SettingsModel::CL86_C;
        result.owner = owner.identity();
        result.sequence = uint32_t(id.slot) + 1;
        result.generation = id.generation;
        result.acquiredUs = sequence->acquiredUs;
        return result;
    }
    L::CommunicationNmt nmt() const {
        const C::NodeSnapshot* node = services.node(0);
        CHECK(node && node->nmtTime.present);
        L::CommunicationNmt result;
        result.known = true;
        result.state = node->nmt;
        result.time = node->nmtTime;
        return result;
    }
    void grant(const C::SdoTransfer& transfer) {
        if (transfer.direction == C::SdoDirection::UPLOAD &&
            transfer.readEffect != C::SdoReadEffect::UNQUALIFIED)
            return;
        if (grantOperation == transfer.operation && grantStep == transfer.step)
            return;
        CHECK(environment == Environment::HOST_FAKE);
        CHECK(transfer.direction == C::SdoDirection::UPLOAD
                  ? services.authorizeUploadExperiment(0, transfer, current.nodeGeneration,
                                                       ++authorization, now)
                  : services.authorizeDownloadExperiment(0, transfer, current.nodeGeneration,
                                                         ++authorization, now));
        grantOperation = transfer.operation;
        grantStep = transfer.step;
    }
    void tick() {
        CHECK(owner.service(++now, OwnerRig::budget()));
        for (; served < wire.sentCount; ++served) {
            M::Frame reply;
            if (drive.answer(wire.sent[served], reply))
                wire.push(reply, now + 1);
        }
        session.service(now);
    }
    C::SdoCompletion exchange(const C::SdoTransfer& transfer) {
        grant(transfer);
        CHECK(session.admitPrepared(transfer, now) == O::BusAdmission::ACCEPTED);
        C::SdoCompletion result;
        for (unsigned i = 0; i < 200; ++i) {
            tick();
            if (session.settle(result))
                return result;
        }
        CHECK(false);
        return result;
    }
    C::SdoCompletion upload(uint16_t index, uint8_t subindex, uint8_t width, uint32_t operation,
                            uint32_t step) {
        // Exact documented scalar baseline through the public native service boundary;
        // no arbitrary object route is exposed to a console or installed profile.
        C::SdoTransfer transfer;
        transfer.channel = current.channel;
        transfer.operation = transfer.request = operation;
        transfer.step = step;
        transfer.index = index;
        transfer.subindex = subindex;
        transfer.size = width;
        transfer.requirePreOperational = true;
        transfer.requireUploadReview = true;
        transfer.deadlineUs = DEADLINE;
        transfer.queueTimeoutUs = transfer.responseTimeoutUs = 100;
        return exchange(transfer);
    }
    void acquire(L::ReadContext& read) {
        while (read.state == M::ReadState::ACTIVE) {
            L::PreparedRead next;
            CHECK(L::nextRead(read, now, next) && next.ready);
            const C::SdoCompletion result = exchange(next.transfer);
            CHECK(L::advanceRead(read, result, now));
        }
        CHECK(read.state == M::ReadState::SUCCEEDED);
    }
    void persistencePrerequisites(L::PersistencePrerequisites& result) {
        uint32_t operation = session.beginOperation(DEADLINE, now);
        CHECK(operation &&
              L::prepareIdentity(result.identity, current, operation, now, DEADLINE, options()));
        acquire(result.identity);
        operation = session.beginOperation(DEADLINE, now);
        CHECK(operation &&
              L::prepareState(result.state, current, operation, now, DEADLINE, options(), STATE));
        acquire(result.state);
        operation = session.beginOperation(DEADLINE, now);
        CHECK(operation &&
              L::prepareDriverRead(result.configuration, current, operation, now, DEADLINE,
                                   options(), L::DriverGroup::MOTION_PARAMETERS));
        acquire(result.configuration.read);
    }
    void prepareCommunication(L::CommunicationContext& output, uint32_t timeoutMs = 0) {
        const uint32_t baselineOperation = session.beginOperation(DEADLINE, now);
        CHECK(baselineOperation);
        const C::SdoCompletion baseline = upload(0x1017, 0, 2, baselineOperation, 1);
        CHECK(baseline.outcome == C::SdoOutcome::SUCCESS && baseline.size == 2);
        const L::AxisReservation held = reservation();
        L::CommunicationReview review;
        review.target = current;
        review.scope = L::CommunicationScope::ISOLATED_EXPERIMENT;
        review.fixtureRecord = 11;
        review.firmwareRecord = 22;
        review.procedureRecord = 33;
        review.authorizationRecord = 44;
        L::CommunicationRequest request;
        request.producerTimeRaw = 1000;
        const uint64_t cutoff = timeoutMs ? now + uint64_t(timeoutMs) * 1000 : DEADLINE;
        CHECK(L::prepareCommunication(output, held, review, request, baseline, nmt(),
                                      session.nextOperation(), now, cutoff, options(), AGE));
    }
    void preparePersistence(L::PersistenceContext& output, bool factory = false,
                            uint32_t timeoutMs = 0) {
        L::PersistencePrerequisites prior;
        persistencePrerequisites(prior);
        const L::AxisReservation held = reservation();
        L::PersistenceReview review;
        review.target = current;
        review.kind = factory ? L::PersistenceKind::FACTORY_RESTORE : L::PersistenceKind::SAVE;
        review.fixtureRecord = 11;
        review.firmwareRecord = 22;
        review.procedureRecord = 33;
        review.authorizationRecord = 44;
        review.backupRecord = 55;
        review.routeBackRecord = 66;
        const auto prepare = factory ? L::prepareFactoryRestore : L::prepareSave;
        const uint64_t cutoff = timeoutMs ? now + uint64_t(timeoutMs) * 1000 : DEADLINE;
        CHECK(prepare(output, held, current, review, prior, session.nextOperation(), 1, now, cutoff,
                      options(), AGE));
    }
    void preparePdo(L::Tpdo1ConfigurationContext& output, bool map = false,
                    uint32_t timeoutMs = 0) {
        static const uint16_t indices[9] = {0x1800, 0x1800, 0x1800, 0x1800, 0x1a00,
                                            0x1a00, 0x1a00, 0x1a00, 0x1a00};
        static const uint8_t subindices[9] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
        static const uint8_t widths[9] = {4, 1, 2, 2, 1, 4, 4, 4, 4};
        const uint32_t baselineOperation = session.beginOperation(DEADLINE, now);
        CHECK(baselineOperation);
        C::SdoCompletion baseline[9];
        for (unsigned i = 0; i < 9; ++i) {
            baseline[i] = upload(indices[i], subindices[i], widths[i], baselineOperation, i + 1);
            CHECK(baseline[i].outcome == C::SdoOutcome::SUCCESS);
        }
        const L::AxisReservation held = reservation();
        L::Tpdo1ConfigurationReview review;
        review.target = current;
        review.scope = L::PdoConfigurationScope::ISOLATED_EXPERIMENT;
        review.fixtureRecord = 11;
        review.firmwareRecord = 22;
        review.procedureRecord = 33;
        review.authorizationRecord = 44;
        L::Tpdo1ConfigurationRequest request;
        request.transition =
            map ? L::Tpdo1Transition::MAP3_TO_MAP2 : L::Tpdo1Transition::EVENT_TIMER;
        request.eventTimer = map ? L::Tpdo1EventTimerRaw::RAW_0 : L::Tpdo1EventTimerRaw::RAW_100;
        const L::CommunicationNmt observed = nmt();
        const uint64_t cutoff = timeoutMs ? now + uint64_t(timeoutMs) * 1000 : DEADLINE;
        CHECK(L::prepareTpdo1Configuration(output, held, review, request, baseline, 9,
                                           observed.state, observed.time, session.nextOperation(),
                                           now, cutoff, options(), AGE));
    }
    void grantNext() {
        if (!commissioning.retained())
            return;
        L::PreparedRead next;
        if (const L::CommunicationContext* context = commissioning.communication()) {
            const L::CommunicationNmt observed = nmt();
            CHECK(L::nextCommunication(*context, context->reservation, current.channel.binding,
                                       observed, now, next));
        } else if (const L::PersistenceContext* context = commissioning.persistence()) {
            CHECK(L::nextPersistence(*context, context->reservation, current.channel.binding, now,
                                     next));
        } else if (const L::Tpdo1ConfigurationContext* context = commissioning.pdo()) {
            const L::CommunicationNmt observed = nmt();
            CHECK(L::nextPdoConfiguration(*context, context->reservation, current.channel.binding,
                                          observed.state, observed.time, now, next));
        }
        if (next.ready && owner.matchesChannel(next.transfer.channel, current.nodeGeneration))
            grant(next.transfer);
    }
    void progress() {
        grantNext();
        tick();
        CHECK(commissioning.service(now));
    }
    void finish() {
        for (unsigned i = 0; i < 1000 && commissioning.active(); ++i)
            progress();
        CHECK(!commissioning.active());
    }
};
struct ConsoleFixture {
    struct Bytes {
        char output[8192] = {};
        size_t used = 0;
        static bool write(void* context, uint8_t byte) {
            Bytes& bytes = *static_cast<Bytes*>(context);
            CHECK(bytes.used + 1 < sizeof bytes.output);
            bytes.output[bytes.used++] = static_cast<char>(byte);
            bytes.output[bytes.used] = 0;
            return true;
        }
        E::ProbeCallbacks callbacks() {
            E::ProbeCallbacks result;
            result.context = this;
            result.writeByte = write;
            return result;
        }
    } bytes;
    Rig rig;
    E::ProbePlatform platform;
    E::ProbeApp app;
    E::ProbeConsole console;
    ConsoleFixture()
        : platform(bytes.callbacks()), app(rig.owner, platform), console(app, rig.owner, platform) {
        CHECK(app.select(rig.current));
        CHECK(console.attachCommissioning(rig.commissioning));
    }
    void command(const char* text) {
        for (const char* p = text; *p; ++p)
            console.feed(static_cast<uint8_t>(*p), rig.now);
        console.feed('\n', rig.now);
    }
    void drain() {
        for (unsigned i = 0; i < 400; ++i) {
            rig.grantNext();
            rig.tick();
            platform.service();
            console.service(rig.now); // Sole bridge progression in this path.
        }
        CHECK(console.drained() && platform.stats().outputUsed == 0);
        CHECK(console.stats().outputDropped == 0);
        for (const char* line = bytes.output; *line;) {
            const char* end = std::strchr(line, '\n');
            CHECK(end && size_t(end - line + 1) < E::ProbeConsole::RECORD_CAPACITY);
            CHECK(*line == '{' && end[-1] == '}');
            line = end + 1;
        }
    }
    void record(uint32_t id, const char* field, bool terminal = false) {
        char identity[40];
        std::snprintf(identity, sizeof identity, "\"id\":%lu,", static_cast<unsigned long>(id));
        for (const char* line = bytes.output; *line;) {
            const char* end = std::strchr(line, '\n');
            CHECK(end);
            const char* request = std::strstr(line, identity);
            const char* type = std::strstr(line, "\"type\":\"terminal\"");
            if (request && request < end && (!terminal || (type && type < end))) {
                const char* found = std::strstr(line, field);
                if (!found || found >= end)
                    std::fprintf(stderr, "Missing [%s] for %s:\n%s", field, identity, bytes.output);
                CHECK(found && found < end);
                return;
            }
            line = end + 1;
        }
        std::fprintf(stderr, "Missing record %s in:\n%s", identity, bytes.output);
        CHECK(false);
    }
};
void invalidPreparedKindAndIdentityDoNotAdmitWork() {
    Rig rig;
    L::CommunicationContext prepared;
    rig.prepareCommunication(prepared);
    const uint32_t operation = rig.session.nextOperation();
    const unsigned sent = rig.wire.sentCount;
    CHECK(rig.commissioning.armCommunication(prepared));
    const Saved<L::CommunicationContext> armed(*rig.commissioning.communication());
    CHECK(!rig.commissioning.requestPersistence(rig.now, DEADLINE));
    CHECK(!rig.commissioning.requestPdo(rig.now, DEADLINE));
    CHECK(!rig.commissioning.requestCommunication(rig.now + 1, DEADLINE));
    CHECK(!rig.commissioning.requestCommunication(rig.now, DEADLINE - 1));
    armed.unchanged(*rig.commissioning.communication());
    CHECK(rig.commissioning.armed() && !rig.commissioning.retained());
    CHECK(rig.session.nextOperation() == operation && rig.wire.sentCount == sent);
    CHECK(rig.commissioning.release());
    L::CommunicationContext invalid = prepared;
    invalid.reservation.model = L::SettingsModel::OL86_C;
    CHECK(rig.commissioning.armCommunication(invalid));
    CHECK(!rig.commissioning.requestCommunication(rig.now, DEADLINE));
    CHECK(rig.session.nextOperation() == operation && rig.wire.sentCount == sent);
    CHECK(rig.commissioning.release());
    invalid = prepared;
    ++invalid.operation;
    CHECK(rig.commissioning.armCommunication(invalid));
    CHECK(!rig.commissioning.requestCommunication(rig.now, DEADLINE));
    CHECK(rig.session.nextOperation() == operation && rig.wire.sentCount == sent);
    CHECK(rig.commissioning.release());
    invalid = prepared;
    invalid.transmissionNoted = true;
    CHECK(!rig.commissioning.armCommunication(invalid));
    CHECK(rig.wire.guards == 0 && rig.drive.downloads == 0);
}
void heartbeatActualHandoffConfirmationAndFollow() {
    Rig rig;
    L::CommunicationContext prepared;
    rig.prepareCommunication(prepared);
    const Saved<L::CommunicationContext> original(prepared);
    const M::Binding before = rig.current.channel.binding;
    const unsigned uploads = rig.drive.uploads;
    CHECK(rig.commissioning.armCommunication(prepared));
    CHECK(rig.commissioning.requestCommunication(rig.now, DEADLINE));
    rig.grantNext();
    CHECK(rig.commissioning.service(rig.now)); // Queue only, without possible-TX notice.
    CHECK(M::sameBinding(before, rig.current.channel.binding));
    L::PreparedRead next;
    CHECK(L::nextCommunication(*rig.commissioning.communication(), prepared.reservation,
                               rig.current.channel.binding, rig.nmt(), rig.now, next) &&
          next.ready);
    C::SdoTransfer wrong = next.transfer;
    wrong.data[0] ^= 1;
    CHECK(!rig.commissioning.preflight(rig.session.sequence(), wrong, rig.now));
    CHECK(!rig.commissioning.communication()->transmissionNoted);
    rig.finish();
    const L::CommunicationContext& result = *rig.commissioning.communication();
    CHECK(result.state == L::CommunicationState::SUCCEEDED && result.writeAcknowledged);
    CHECK(result.readbackMatched && result.readbackRaw == 1000);
    CHECK(result.writeOutcome == M::ReadOutcome::SUCCESS && result.confirmationPresent);
    CHECK(result.restart == L::CommunicationRequirement::UNKNOWN &&
          result.persistence == L::CommunicationRequirement::UNKNOWN);
    CHECK(rig.drive.downloads == 1 && rig.drive.uploads == uploads + 1 && rig.wire.guards == 1);
    CHECK(rig.current.channel.binding.configurationGeneration ==
          before.configurationGeneration + 1);
    CHECK(rig.owner.matchesChannel(rig.current.channel, rig.current.nodeGeneration));
    CHECK(!M::sameBinding(rig.session.binding(), rig.current.channel.binding));
    L::CommunicationResult observation;
    CHECK(L::getCommunication(result, result.reservation, rig.current, rig.now, observation));
    CHECK(observation.producerTimeRaw == 1000);
    const unsigned sent = rig.wire.sentCount;
    CHECK(rig.commissioning.finishBinding(rig.now) && rig.commissioning.release());
    CHECK(M::sameBinding(rig.session.binding(), rig.current.channel.binding));
    CHECK(!rig.commissioning.communication() && rig.wire.sentCount == sent);
    original.unchanged(prepared);
}
void lostHeartbeatReplyRetainsOneAttempt() {
    Rig rig;
    L::CommunicationContext prepared;
    rig.prepareCommunication(prepared);
    rig.drive.fault = Dictionary::Fault::LOST;
    rig.drive.faultIndex = 0x1017;
    CHECK(rig.commissioning.armCommunication(prepared));
    CHECK(rig.commissioning.requestCommunication(rig.now, DEADLINE));
    rig.finish();
    const L::CommunicationContext& result = *rig.commissioning.communication();
    CHECK(result.state == L::CommunicationState::FAILED && result.uncertain &&
          result.effectsPossible);
    CHECK(result.writeOutcome == M::ReadOutcome::SERVICE_TIMEOUT && !result.confirmationPresent);
    CHECK(result.writeEvidence.transmissionPossible && !result.writeAcknowledged);
    CHECK(rig.drive.find(0x1017).raw == 1000 && rig.drive.downloads == 1);
    const Saved<L::CommunicationContext> retained(result);
    const unsigned sent = rig.wire.sentCount;
    for (unsigned i = 0; i < 5; ++i)
        CHECK(rig.commissioning.service(rig.now));
    retained.unchanged(*rig.commissioning.communication());
    CHECK(rig.wire.sentCount == sent);
    CHECK(!rig.commissioning.requestCommunication(rig.now, DEADLINE));
    CHECK(!rig.commissioning.finishBinding(rig.now));
    CHECK(rig.commissioning.release());
    CHECK(!rig.owner.matchesChannel(rig.current.channel, rig.current.nodeGeneration));
}
void saveAndFactoryKeepDistinctLiteralAndKnowledge() {
    for (unsigned factory = 0; factory < 2; ++factory) {
        Rig rig;
        L::PersistenceContext prepared;
        rig.preparePersistence(prepared, factory != 0);
        const Saved<L::PersistenceContext> original(prepared);
        CHECK(rig.commissioning.armPersistence(prepared));
        CHECK(rig.commissioning.requestPersistence(rig.now, DEADLINE));
        const M::Generation generation = rig.current.channel.binding.configurationGeneration;
        rig.finish();
        const L::PersistenceContext& result = *rig.commissioning.persistence();
        CHECK(result.state == L::PersistenceState::ACKNOWLEDGED && result.attempts == 1);
        CHECK(result.execution == M::ActionExecution::ACKNOWLEDGED && result.writeEvidencePresent);
        CHECK(result.kind ==
              (factory ? L::PersistenceKind::FACTORY_RESTORE : L::PersistenceKind::SAVE));
        CHECK(rig.drive.find(0x2300).raw == (factory ? 1u : 2u) && rig.drive.downloads == 1);
        CHECK(!result.verificationKnown && result.survivedRestart == 0);
        CHECK(result.restart == L::PersistenceRequirement::UNKNOWN &&
              result.completeSavedSet == L::PersistenceRequirement::UNKNOWN);
        CHECK(rig.current.channel.binding.configurationGeneration == generation + 1);
        const unsigned sent = rig.wire.sentCount;
        CHECK(rig.commissioning.service(rig.now) && rig.commissioning.finishBinding(rig.now));
        CHECK(rig.commissioning.release() && rig.wire.sentCount == sent);
        original.unchanged(prepared);
    }
}
void pdoTimerRoundTripKeepsSampleQualificationSeparate() {
    Rig rig;
    L::Tpdo1ConfigurationContext prepared;
    rig.preparePdo(prepared);
    const unsigned uploads = rig.drive.uploads;
    const M::Generation generation = rig.current.channel.binding.configurationGeneration;
    CHECK(rig.commissioning.armPdo(prepared));
    CHECK(rig.commissioning.requestPdo(rig.now, DEADLINE));
    rig.finish();
    const L::Tpdo1ConfigurationContext& result = *rig.commissioning.pdo();
    CHECK(result.state == L::PdoConfigurationState::SUCCEEDED && result.mapConfirmed);
    CHECK(result.acknowledgedMask == 1 && result.readbackCount == 9 && !result.uncertain);
    CHECK(rig.drive.downloads == 1 && rig.drive.uploads == uploads + 9);
    CHECK(rig.drive.find(0x1800, 5).raw == 100 && rig.drive.find(0x1a00).raw == 3);
    CHECK(rig.current.channel.binding.configurationGeneration == generation + 1);
    C::PdoMapEvidence contents;
    L::Tpdo1EventTimerRaw timer;
    CHECK(L::getTpdo1Configuration(result, result.reservation, rig.current.channel.binding, rig.now,
                                   2, 700, contents, timer));
    CHECK(timer == L::Tpdo1EventTimerRaw::RAW_100);
    CHECK(!rig.services.pdo(0)); // The bridge never installs a map or qualifies samples.
    CHECK(rig.commissioning.finishBinding(rig.now) && rig.commissioning.release());
}
void missingConfirmationPermissionLeavesNoOrphan() {
    Rig rig;
    L::CommunicationContext prepared;
    rig.prepareCommunication(prepared);
    CHECK(rig.commissioning.armCommunication(prepared));
    CHECK(rig.commissioning.requestCommunication(rig.now, DEADLINE));
    for (unsigned i = 0;
         i < 20 && rig.commissioning.communication()->state != L::CommunicationState::READING_BACK;
         ++i)
        rig.progress();
    CHECK(rig.commissioning.communication()->state == L::CommunicationState::READING_BACK);
    CHECK(rig.commissioning.communication()->writeAcknowledged && !rig.session.busy());
    // This exact Q20 confirmation requires its own grant. Deliberately omit it.
    CHECK(!rig.commissioning.service(rig.now));
    const L::CommunicationContext& result = *rig.commissioning.communication();
    CHECK(!rig.commissioning.active() && !rig.session.busy());
    CHECK(result.state == L::CommunicationState::FAILED && result.writeAcknowledged);
    CHECK(result.writeOutcome == M::ReadOutcome::SUCCESS && result.uncertain);
    CHECK(!result.confirmationPresent && rig.drive.downloads == 1 && rig.drive.uploads == 1);
    const O::OwnerResources resources = rig.owner.resources();
    CHECK(resources.queued == 0 && resources.active == 0 && resources.retained == 0);
    CHECK(rig.commissioning.release());
}
void controllerRetirementPrecedesConfirmation() {
    Rig rig;
    L::CommunicationContext prepared;
    rig.prepareCommunication(prepared);
    const unsigned downloadFrame = rig.wire.sentCount;
    rig.wire.autoReport = false;
    rig.wire.retirement = false;
    CHECK(rig.commissioning.armCommunication(prepared));
    CHECK(rig.commissioning.requestCommunication(rig.now, DEADLINE));
    for (unsigned i = 0; i < 20 && rig.commissioning.communication()->state !=
                                       L::CommunicationState::AWAITING_CONFIRMATION;
         ++i)
        rig.progress();
    CHECK(rig.commissioning.communication()->writeAcknowledged);
    CHECK(!rig.owner.localTxRetired(0));
    CHECK(rig.commissioning.service(rig.now));
    CHECK(rig.commissioning.communication()->state == L::CommunicationState::AWAITING_CONFIRMATION);
    CHECK(!rig.owner.matchesChannel(rig.current.channel, rig.current.nodeGeneration));
    CHECK(rig.drive.uploads == 1 && rig.drive.downloads == 1);
    rig.wire.report(downloadFrame);
    rig.wire.autoReport = true;
    rig.wire.retirement = true;
    rig.finish();
    CHECK(rig.commissioning.communication()->state == L::CommunicationState::SUCCEEDED);
    CHECK(rig.drive.downloads == 1 && rig.drive.uploads == 2);
    CHECK(rig.owner.localTxRetired(0) && !rig.session.busy());
}
void lostSaveReplyCannotBeReplayed() {
    Rig rig;
    L::PersistenceContext prepared;
    rig.preparePersistence(prepared);
    rig.drive.fault = Dictionary::Fault::LOST;
    rig.drive.faultIndex = 0x2300;
    CHECK(rig.commissioning.armPersistence(prepared));
    CHECK(rig.commissioning.requestPersistence(rig.now, DEADLINE));
    rig.finish();
    const L::PersistenceContext& result = *rig.commissioning.persistence();
    CHECK(result.state == L::PersistenceState::FAILED && result.attempts == 1);
    CHECK(result.execution == M::ActionExecution::UNKNOWN && result.uncertain &&
          result.effectsPossible);
    CHECK(result.outcome == M::ReadOutcome::SERVICE_TIMEOUT && result.writeEvidencePresent);
    CHECK(rig.drive.find(0x2300).raw == 2 && rig.drive.downloads == 1);
    const Saved<L::PersistenceContext> retained(result);
    const unsigned sent = rig.wire.sentCount;
    CHECK(rig.commissioning.service(rig.now));
    CHECK(!rig.commissioning.requestPersistence(rig.now, DEADLINE));
    CHECK(!rig.commissioning.finishBinding(rig.now));
    retained.unchanged(*rig.commissioning.persistence());
    CHECK(rig.commissioning.release() && rig.wire.sentCount == sent);
}
void pdoPartialMapAbortKeepsUnsignedNativeEvidence() {
    Rig rig;
    L::Tpdo1ConfigurationContext prepared;
    rig.preparePdo(prepared, true);
    rig.drive.fault = Dictionary::Fault::ABORT;
    rig.drive.faultIndex = 0x1a00;
    rig.drive.faultSubindex = 0;
    const M::Generation generation = rig.current.channel.binding.configurationGeneration;
    CHECK(rig.commissioning.armPdo(prepared));
    CHECK(rig.commissioning.requestPdo(rig.now, DEADLINE));
    rig.finish();
    const L::Tpdo1ConfigurationContext& result = *rig.commissioning.pdo();
    CHECK(result.state == L::PdoConfigurationState::FAILED && result.acknowledgedMask == 1);
    CHECK(result.failedStep == 2 && result.firstEffectStep == 1 && result.uncertain);
    CHECK(result.outcome == M::ReadOutcome::PROTOCOL_REJECTED && !result.mapConfirmed);
    CHECK(result.evidence.abortValid && result.evidence.abortCode == 0xf1234567u);
    CHECK(rig.drive.find(0x1800, 1).raw == 0x80000181u);
    CHECK(rig.drive.find(0x1a00).raw == 3 && rig.drive.downloads == 2);
    CHECK(rig.current.channel.binding.configurationGeneration == generation + 1);
    CHECK(!rig.session.busy() && rig.owner.resources().retained == 0);
    const unsigned sent = rig.wire.sentCount;
    CHECK(rig.commissioning.service(rig.now));
    CHECK(rig.commissioning.release() && rig.wire.sentCount == sent);
}
void bootAfterPossibleSavePreservesNativeFailure() {
    Rig rig;
    L::PersistenceContext prepared;
    rig.preparePersistence(prepared);
    rig.drive.fault = Dictionary::Fault::LOST;
    rig.drive.faultIndex = 0x2300;
    CHECK(rig.commissioning.armPersistence(prepared));
    CHECK(rig.commissioning.requestPersistence(rig.now, DEADLINE));
    rig.progress(); // Queue the exact prepared write.
    rig.progress(); // Actual possible TX, with its reply deliberately missing.
    CHECK(rig.commissioning.active() && rig.drive.downloads == 1);
    CHECK(rig.commissioning.persistence()->effectsPossible);
    M::Frame boot = OwnerRig::frame(0x701, 1);
    rig.wire.push(boot, rig.now + 1);
    rig.tick();
    CHECK(rig.commissioning.service(rig.now));
    const L::PersistenceContext& result = *rig.commissioning.persistence();
    CHECK(result.state == L::PersistenceState::FAILED && !rig.commissioning.active());
    CHECK(result.writeEvidencePresent && result.writeEvidence.outcome == C::SdoOutcome::REBOOT);
    CHECK(result.execution == M::ActionExecution::UNKNOWN && result.uncertain &&
          result.attempts == 1);
    CHECK(result.before.identity.deviceType == 0x00040192 && result.review.fixtureRecord == 11);
    CHECK(rig.commissioning.lastCompletion() &&
          rig.commissioning.lastCompletion()->outcome == C::SdoOutcome::REBOOT);
    CHECK(!rig.commissioning.finishBinding(rig.now));
    CHECK(rig.commissioning.release() && rig.drive.downloads == 1);
}
void consolePreparedFamiliesRetainSuccessAndUncertainty() {
    for (unsigned family = 0; family < 3; ++family)
        for (unsigned lost = 0; lost < 2; ++lost) {
            ConsoleFixture fixture;
            Rig& rig = fixture.rig;
            const char* wrong = nullptr;
            const char* command = nullptr;
            const char* success = nullptr;
            const char* failure = nullptr;
            if (family == 0) {
                L::CommunicationContext prepared;
                rig.prepareCommunication(prepared, 1000);
                CHECK(rig.commissioning.armCommunication(prepared));
                wrong = "@601 persistence save parameters";
                command = "@602 communication heartbeat 1000";
                success = "\"outcome\":\"READBACK_MATCHED\"";
                failure = "\"outcome\":\"COMMUNICATION_FAILED\"";
                rig.drive.faultIndex = 0x1017;
            } else if (family == 1) {
                L::PersistenceContext prepared;
                rig.preparePersistence(prepared, false, 1000);
                CHECK(rig.commissioning.armPersistence(prepared));
                wrong = "@601 persistence restore factory settings";
                command = "@602 persistence save parameters";
                success = "\"outcome\":\"ACKNOWLEDGED\"";
                failure = "\"outcome\":\"PERSISTENCE_FAILED\"";
                rig.drive.faultIndex = 0x2300;
            } else {
                L::Tpdo1ConfigurationContext prepared;
                rig.preparePdo(prepared, false, 1000);
                CHECK(rig.commissioning.armPdo(prepared));
                wrong = "@601 pdo apply timer 0";
                command = "@602 pdo apply timer 100";
                success = "\"outcome\":\"PDO_READBACK_MATCHED\"";
                failure = "\"outcome\":\"PDO_CONFIGURATION_FAILED\"";
                rig.drive.faultIndex = 0x1800;
                rig.drive.faultSubindex = 5;
            }
            if (lost)
                rig.drive.fault = Dictionary::Fault::LOST;
            const uint32_t operation = rig.session.nextOperation();
            const unsigned sent = rig.wire.sentCount;
            fixture.command(wrong);
            CHECK(rig.commissioning.armed() && rig.session.nextOperation() == operation);
            CHECK(rig.wire.sentCount == sent && rig.drive.downloads == 0);
            fixture.command(
                command); // Same externally prepared instant, after the rejected intent.
            fixture.drain();
            fixture.record(601, "\"ok\":false");
            fixture.record(602, "\"result\":\"accepted\",\"operation_id\":1");
            fixture.record(602, lost ? failure : success, true);
            fixture.record(602, lost ? "\"uncertain\":true" : "\"ok\":true", true);
            fixture.record(602, "\"command_id\":602,\"operation_id\":1", true);
            CHECK(rig.drive.downloads == 1 && rig.wire.guards == 1 && rig.commissioning.retained());
            if (family == 0) {
                fixture.record(
                    602, lost ? "\"write_acknowledged\":false" : "\"write_acknowledged\":true",
                    true);
                fixture.record(
                    602, lost ? "\"readback_matched\":false" : "\"readback_matched\":true", true);
                fixture.record(602, "\"restart\":null,\"persistent\":null", true);
            } else if (family == 1) {
                fixture.record(602, "\"attempts\":1,\"durability_known\":false,\"restart\":null",
                               true);
                CHECK(rig.commissioning.persistence()->attempts == 1);
            } else {
                fixture.record(602, lost ? "\"acknowledged_mask\":0" : "\"acknowledged_mask\":1",
                               true);
                fixture.record(602, lost ? "\"map_confirmed\":false" : "\"map_confirmed\":true",
                               true);
                fixture.record(602, "\"samples_qualified\":false", true);
            }
            const unsigned finalSent = rig.wire.sentCount;
            fixture.command("@603 result 1");
            fixture.drain();
            fixture.record(603, lost ? failure : success);
            fixture.record(603, "\"command_id\":602,\"operation_id\":1");
            CHECK(rig.wire.sentCount == finalSent);
            fixture.command("@604 release 1");
            fixture.drain();
            fixture.record(604, "\"ok\":true");
            CHECK(!rig.commissioning.retained() && rig.wire.sentCount == finalSent);
        }
}
void staleConsoleNodeEpochCannotStartCurrentPreparedWork() {
    ConsoleFixture fixture;
    Rig& rig = fixture.rig;
    const M::Generation selectedEpoch = fixture.app.selected()->nodeGeneration;
    M::Frame boot = OwnerRig::frame(0x701, 1);
    rig.wire.push(boot, rig.now + 1);
    rig.tick();
    CHECK(rig.session.close());
    rig.current.nodeGeneration = rig.services.node(0)->route.generation;
    CHECK(rig.current.nodeGeneration != selectedEpoch);

    // Reconcile the real quarantined channel through its explicit accepted drain marker.
    // HOST_FAKE supplies the same sole-client/ordered-server premises as the owner tests.
    M::Frame preop = OwnerRig::frame(0x701, 1);
    preop.data[0] = 127;
    rig.wire.push(preop, rig.now + 1);
    rig.tick();
    L::ReadContext marker;
    CHECK(L::prepareProbe(marker, rig.current, 1, rig.now, DEADLINE, options()));
    L::PreparedRead work;
    CHECK(L::nextRead(marker, rig.now, work) && work.ready);
    work.transfer.allowSegmentedUpload = false; // The bounded drain marker requires scalar-only.
    C::SdoDrainPremises premises;
    premises.soleClient = premises.orderedServer = true;
    O::RequestId id;
    CHECK(rig.owner.admitDrainMarker(work.transfer, premises, rig.now, id) ==
          O::BusAdmission::ACCEPTED);
    for (unsigned i = 0; i < 20 && !rig.owner.result(id); ++i)
        rig.tick();
    CHECK(rig.owner.result(id) && rig.owner.result(id)->service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(rig.owner.release(id) && !rig.services.unresolved(0));
    CHECK(rig.session.open(rig.current, L::SettingsModel::CL86_C, 1, rig.now));
    rig.wire.push(preop, rig.now + 1); // Actual fresh NMT evidence under the new reservation.
    rig.tick();
    L::CommunicationContext prepared;
    rig.prepareCommunication(prepared, 1000);
    CHECK(rig.commissioning.armCommunication(prepared));
    CHECK(fixture.app.selected()->nodeGeneration == selectedEpoch);
    CHECK(M::sameBinding(fixture.app.selected()->channel.binding, rig.current.channel.binding));
    CHECK(rig.owner.matchesChannel(rig.current.channel, rig.current.nodeGeneration));
    const uint32_t operation = rig.session.nextOperation();
    const unsigned sent = rig.wire.sentCount;
    const Saved<L::CommunicationContext> armed(*rig.commissioning.communication());
    fixture.command("@701 communication heartbeat 1000");
    CHECK(rig.commissioning.armed() && !rig.commissioning.retained());
    CHECK(rig.session.nextOperation() == operation && rig.wire.sentCount == sent);
    fixture.drain();
    fixture.record(701, "\"ok\":false");
    armed.unchanged(*rig.commissioning.communication());
    CHECK(rig.drive.downloads == 0 && rig.wire.sentCount == sent);
}
void oldConsoleCancelAndReleaseCannotTouchReplacement() {
    ConsoleFixture fixture;
    Rig& rig = fixture.rig;
    L::PersistenceContext original;
    rig.preparePersistence(original, false, 1000);
    CHECK(rig.commissioning.armPersistence(original));
    fixture.command("@801 persistence save parameters");
    CHECK(rig.commissioning.retained());
    const uint32_t oldOperation = rig.commissioning.persistence()->operation;
    rig.finish(); // Settle the actual old helper without giving console.poll a turn.
    CHECK(rig.commissioning.persistence()->state == L::PersistenceState::ACKNOWLEDGED);
    CHECK(rig.commissioning.finishBinding(rig.now) && rig.commissioning.release());

    L::PersistenceContext replacement;
    rig.preparePersistence(replacement);
    CHECK(rig.commissioning.armPersistence(replacement));
    CHECK(rig.commissioning.requestPersistence(rig.now, DEADLINE));
    CHECK(rig.commissioning.persistence()->operation != oldOperation);
    const Saved<L::PersistenceContext> retained(*rig.commissioning.persistence());
    const uint32_t currentOperation = rig.owner.sequence(rig.session.sequence())->operation;
    const unsigned sent = rig.wire.sentCount;
    fixture.command("@802 cancel 1"); // Public direct ingress, deliberately before console.poll.
    retained.unchanged(*rig.commissioning.persistence());
    CHECK(rig.commissioning.active() && rig.wire.sentCount == sent);
    CHECK(rig.owner.sequence(rig.session.sequence())->operation == currentOperation);
    fixture.drain();
    fixture.record(802, "operation_context_replaced");
    fixture.record(801, "\"outcome\":\"INVALIDATED\"", true);
    retained.unchanged(*rig.commissioning.persistence());
    CHECK(rig.commissioning.active() && rig.drive.downloads == 1 && rig.wire.sentCount == sent);

    fixture.command("@803 release 1");
    fixture.drain();
    fixture.record(803, "\"ok\":true");
    retained.unchanged(*rig.commissioning.persistence());
    CHECK(rig.commissioning.retained() && rig.commissioning.active());
    CHECK(rig.owner.resources().queued == 0 && rig.owner.resources().active == 0);
    CHECK(rig.wire.sentCount == sent && rig.drive.downloads == 1);
    CHECK(rig.commissioning.cancel(rig.now) && rig.commissioning.release());
}
} // namespace
int main() {
    invalidPreparedKindAndIdentityDoNotAdmitWork();
    heartbeatActualHandoffConfirmationAndFollow();
    lostHeartbeatReplyRetainsOneAttempt();
    saveAndFactoryKeepDistinctLiteralAndKnowledge();
    pdoTimerRoundTripKeepsSampleQualificationSeparate();
    missingConfirmationPermissionLeavesNoOrphan();
    controllerRetirementPrecedesConfirmation();
    lostSaveReplyCannotBeReplayed();
    pdoPartialMapAbortKeepsUnsignedNativeEvidence();
    bootAfterPossibleSavePreservesNativeFailure();
    consolePreparedFamiliesRetainSuccessAndUncertainty();
    staleConsoleNodeEpochCannotStartCurrentPreparedWork();
    oldConsoleCancelAndReleaseCannotTouchReplacement();
    std::printf("probe_commissioning_app: %u checks passed (HOST_FAKE only)\n", checks);
}
