// SPDX-License-Identifier: MIT
// Prompt 55 retry classes through the portable core alone. Every terminal here is manufactured in
// the shape the owner retains; no owner, controller or drive takes part, and no raw value is a
// device measurement. The library never repeats work: each repeat below is the test's own new
// preparation under a new operation identity, and the test checks what that preparation yields.
#include <MotorControlCAN/canopen/Sync.h>
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>
#include <MotorControlCAN/profiles/lichuan_c/Homing.h>
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>
#include <MotorControlCAN/profiles/lichuan_c/Persistence.h>
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <MotorControlCAN/profiles/lichuan_c/Velocity.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
static unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

namespace {
const M::RetryClass N = M::RetryClass::NOT_REPEATABLE;
const M::RetryClass R = M::RetryClass::REPEATABLE_READ;
const uint64_t SPAN = 100000;

// Same object, its own snapshot: compares bytes, padding included, so never two objects.
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) {
        std::memcpy(bytes, static_cast<const void*>(&value), sizeof value);
    }
    bool same(const T& value) const {
        return !std::memcmp(bytes, static_cast<const void*>(&value), sizeof value);
    }
};
L::ReadTarget target() {
    L::ReadTarget value;
    value.channel.binding.logicalAxisId = 101;
    value.channel.binding.endpoint.profile = 41;
    value.channel.binding.endpoint.address = 1;
    value.channel.binding.bindingGeneration = 3;
    value.channel.binding.configurationGeneration = 7;
    value.channel.clientToServerId = 0x601;
    value.channel.serverToClientId = 0x581;
    value.nodeGeneration = 11;
    return value;
}
L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = value.responseTimeoutUs = 100;
    return value;
}
uint8_t blocks(L::StateBlock a) { return uint8_t(1u << unsigned(a)); }
uint8_t blocks(L::StateBlock a, L::StateBlock b) { return uint8_t(blocks(a) | blocks(b)); }
// The owner's retained success for exactly this work, admitted and completed at the given times.
C::SdoCompletion success(const C::SdoTransfer& work, uint32_t raw, uint64_t admitted,
                         uint64_t completed) {
    C::SdoCompletion e;
    e.transfer = work;
    e.outcome = C::SdoOutcome::SUCCESS;
    e.admittedUs = admitted;
    e.completedUs = completed;
    e.nodeGeneration = target().nodeGeneration;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = true;
    e.protocolAcknowledged = e.effectPossible = true;
    if (work.direction == C::SdoDirection::UPLOAD) {
        e.size = work.size;
        for (unsigned i = 0; i < work.size && i < 4; ++i)
            e.data[i] = uint8_t(raw >> (8 * i));
    }
    return e;
}
// A response timeout after the controller reported the frame sent: it may have acted, unconfirmed.
C::SdoCompletion lost(const C::SdoTransfer& work, uint64_t admitted, uint64_t completed) {
    C::SdoCompletion e;
    e.transfer = work;
    e.outcome = C::SdoOutcome::TIMEOUT;
    e.admittedUs = admitted;
    e.completedUs = completed;
    e.nodeGeneration = target().nodeGeneration;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = true;
    e.effectPossible = e.effectUncertain = true;
    return e;
}
// Answer every upload of a prepared read in order, then require success.
void complete(L::ReadContext& c, uint64_t& now, const uint32_t* raws, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        L::PreparedRead work;
        CHECK(L::nextRead(c, now, work) && work.ready);
        const C::SdoCompletion e = success(work.transfer, raws[i], now, now + 1);
        now += 2;
        CHECK(L::advanceRead(c, e, now));
    }
    CHECK(c.state == M::ReadState::SUCCEEDED);
}
uint16_t word(const C::SdoTransfer& t) { return uint16_t(t.data[0] | (uint16_t(t.data[1]) << 8)); }
bool controlword(const C::SdoTransfer& t, uint16_t value) {
    return t.index == 0x6040 && !t.subindex && t.direction == C::SdoDirection::DOWNLOAD &&
           t.size == 2 && word(t) == value;
}

// An empty or reset context of every prepared operation states N, the default.
void defaultsAreNotRepeatable() {
    CHECK(M::RetryClass() == N && static_cast<unsigned>(N) == 0);
    static L::ReadContext read;
    static L::DriverContext driver;
    static L::CommunicationReadContext communicationRead;
    static L::DriverSettingsContext settings;
    static L::CommunicationContext communication;
    static L::Tpdo1ConfigurationContext pdo;
    static L::PersistenceContext persistence;
    static L::ActionContext action;
    static L::BestEffortStop bestEffort;
    static L::MoveContext move;
    static L::VelocityContext velocity;
    static L::HomeContext home;
    static C::SyncContext sync;
    CHECK(read.retryClass == N && driver.read.retryClass == N);
    CHECK(communicationRead.read.retryClass == N);
    CHECK(settings.retryClass == N && settings.readback.read.retryClass == N);
    CHECK(communication.retryClass == N && pdo.retryClass == N && persistence.retryClass == N);
    CHECK(action.retryClass == N && action.read.retryClass == N && bestEffort.retryClass == N);
    CHECK(move.retryClass == N && velocity.retryClass == N && home.retryClass == N);
    CHECK(sync.retryClass == N);
    // A reset forgets a preparation's class with the rest of its history.
    uint64_t now = 100;
    CHECK(L::prepareProbe(read, target(), 1, now, now + SPAN, options()) && read.retryClass == R);
    L::resetRead(read);
    CHECK(read.retryClass == N && read.state == M::ReadState::EMPTY);
}

// R only for the two repeatable effects; the class never upgrades or rewrites an effect.
void repeatableEffects() {
    CHECK(C::repeatableReadEffect(C::SdoReadEffect::REVIEWED_NON_CHANGING));
    CHECK(C::repeatableReadEffect(C::SdoReadEffect::ACCEPTED_UNQUALIFIED));
    CHECK(!C::repeatableReadEffect(C::SdoReadEffect::UNQUALIFIED));
    CHECK(!C::repeatableReadEffect(static_cast<C::SdoReadEffect>(3)));
    CHECK(!C::repeatableReadEffect(static_cast<C::SdoReadEffect>(0xff)));
}

// The class of a read follows the documented DEC-1 list, which holds the identity fields, 1000,
// 2100..2102, 6041, 6061, 6064, 606C and the enable's 607A, and nothing else. Every other read in
// this profile carries an UNQUALIFIED upload and is N: 1001, 603F and 1003 among them.
void readClasses() {
    static L::ReadContext c;
    const L::ReadTarget t = target();
    uint64_t now = 100;
    uint32_t operation = 1;
    CHECK(L::prepareProbe(c, t, operation++, now, now + SPAN, options()) && c.retryClass == R);
    CHECK(L::prepareIdentity(c, t, operation++, now, now + SPAN, options()) && c.retryClass == R);
    CHECK(L::prepareConfig(c, t, operation++, now, now + SPAN, options()) && c.retryClass == N);
    struct StateCase {
        uint8_t mask;
        M::RetryClass expected;
    };
    const StateCase cases[] = {
        {blocks(L::StateBlock::MONITORS), R},
        {blocks(L::StateBlock::STATUS), R},
        {blocks(L::StateBlock::FEEDBACK), R},
        {blocks(L::StateBlock::MONITORS, L::StateBlock::STATUS), R},
        {uint8_t(blocks(L::StateBlock::MONITORS, L::StateBlock::STATUS) |
                 blocks(L::StateBlock::FEEDBACK)),
         R},
        {blocks(L::StateBlock::ERRORS), N},        // 1001, 603F
        {blocks(L::StateBlock::ERROR_HISTORY), N}, // 1003
        {blocks(L::StateBlock::INPUTS), N},        // 60FD
        {blocks(L::StateBlock::STATUS, L::StateBlock::ERRORS), N},
        {0x3f, N},
    };
    for (const StateCase& k : cases) {
        CHECK(L::prepareState(c, t, operation++, now, now + SPAN, options(), k.mask));
        CHECK(c.retryClass == k.expected);
    }
    // Every native group read is N; their objects are outside DEC-1.
    static L::DriverContext d;
    for (unsigned g = 0; g <= unsigned(L::DriverGroup::PERSISTENCE); ++g) {
        CHECK(L::prepareDriverRead(d, t, operation++, now, now + SPAN, options(),
                                   L::DriverGroup(g)));
        CHECK(d.read.retryClass == N);
    }
    CHECK(L::prepareControlRead(d, t, operation++, now, now + SPAN, options()));
    CHECK(d.read.retryClass == N);
    for (unsigned g = 0; g <= unsigned(L::TuningGroup::EDS_GAINS); ++g) {
        CHECK(L::prepareTuningRead(d, t, operation++, now, now + SPAN, options(),
                                   L::TuningGroup(g)));
        CHECK(d.read.retryClass == N);
    }
    static L::CommunicationReadContext k;
    for (unsigned g = 0; g <= unsigned(L::CommunicationGroup::TPDO_MAPPING); ++g) {
        L::ReadOptions o = options();
        o.pdoNumber = g >= unsigned(L::CommunicationGroup::RPDO_COMMUNICATION) ? 1 : 0;
        CHECK(L::prepareCommunicationRead(k, t, operation++, now, now + SPAN, o,
                                          L::CommunicationGroup(g)));
        CHECK(k.read.retryClass == N);
    }

    // An R read still yields its accepted effect, never REVIEWED_NON_CHANGING, and keeps its class
    // through completion. An N read yields UNQUALIFIED for 1001 and 603F.
    CHECK(L::prepareState(c, t, operation++, now, now + SPAN, options(),
                          blocks(L::StateBlock::MONITORS, L::StateBlock::STATUS)));
    for (unsigned i = 0; i < c.count; ++i)
        CHECK(c.values[i].object.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED);
    L::PreparedRead work;
    CHECK(L::nextRead(c, now, work) && work.ready && work.transfer.index == 0x2100);
    CHECK(work.transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
    const uint32_t clean[] = {0, 0xa580, 0xf008, 0x0037, 1};
    complete(c, now, clean, 5);
    CHECK(c.retryClass == R);
    CHECK(L::prepareState(c, t, operation++, now, now + SPAN, options(),
                          blocks(L::StateBlock::ERRORS)));
    CHECK(c.retryClass == N && c.count == 2);
    for (unsigned i = 0; i < c.count; ++i)
        CHECK(c.values[i].object.readEffect == L::ReadEffect::UNQUALIFIED);
    CHECK(L::nextRead(c, now, work) && work.transfer.index == 0x1001 &&
          work.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
    // A failure keeps the class too; it describes the operation, not its outcome.
    CHECK(L::advanceRead(c, M::ReadOutcome::CANCELLED, now) && c.state == M::ReadState::FAILED);
    CHECK(c.retryClass == N && c.uncertain);
}

// A refused preparation leaves the context, its class included, exactly as it was.
void refusedPreparationKeepsClass() {
    static L::ReadContext c;
    const L::ReadTarget t = target();
    const uint64_t now = 100;
    CHECK(L::prepareProbe(c, t, 1, now, now + SPAN, options()) && c.retryClass == R);
    const Saved<L::ReadContext> probe(c);
    CHECK(!L::prepareState(c, t, 2, now, now, options(), blocks(L::StateBlock::ERRORS)));
    CHECK(probe.same(c));
    CHECK(!L::prepareState(c, t, 2, now, now + SPAN, options(), 0x40));
    CHECK(probe.same(c));
    CHECK(!L::prepareState(c, t, 0, now, now + SPAN, options(), blocks(L::StateBlock::ERRORS)));
    CHECK(probe.same(c));
    CHECK(L::prepareState(c, t, 2, now, now + SPAN, options(), blocks(L::StateBlock::ERRORS)));
    CHECK(c.retryClass == N);
    static L::DriverContext d;
    const Saved<L::DriverContext> empty(d);
    CHECK(!L::prepareDriverRead(d, t, 3, now, now + SPAN, options(),
                                static_cast<L::DriverGroup>(99)));
    CHECK(empty.same(d));
    static C::SyncContext sync;
    C::SyncOptions schedule;
    schedule.generation = 1;
    schedule.originUs = now;
    schedule.periodUs = 1000;
    schedule.count = 2;
    CHECK(C::prepareSync(schedule, now, sync) && sync.retryClass == N);
    CHECK(sync.state == C::SyncState::ACTIVE);
    CHECK(C::cancelSync(sync, now));
    const Saved<C::SyncContext> cancelled(sync);
    CHECK(!C::prepareSync(schedule, now, sync)); // The epoch must rise.
    CHECK(cancelled.same(sync));
}

// An application's permission promotes the named objects only; effects and the context stay.
void permissions() {
    static L::ReadContext c;
    const L::ReadTarget t = target();
    uint64_t now = 100;
    CHECK(L::prepareState(c, t, 1, now, now + SPAN, options(), blocks(L::StateBlock::ERRORS)));
    CHECK(c.retryClass == N);
    const Saved<L::ReadContext> prepared(c);
    M::RetryClass out = R;
    CHECK(L::classifyRead(c, nullptr, 0, out) && out == N);
    L::RepeatPermission fault;
    fault.index = 0x603f;
    fault.record = 501;
    L::RepeatPermission reg;
    reg.index = 0x1001;
    reg.record = 502;
    // One object named, the other still unqualified: N.
    out = R;
    CHECK(L::classifyRead(c, &fault, 1, out) && out == N);
    // Both named: R, by permission. The context's own class, descriptors and work are untouched.
    const L::RepeatPermission both[] = {fault, reg};
    CHECK(L::classifyRead(c, both, 2, out) && out == R);
    CHECK(prepared.same(c) && c.retryClass == N);
    for (unsigned i = 0; i < c.count; ++i)
        CHECK(c.values[i].object.readEffect == L::ReadEffect::UNQUALIFIED);
    L::PreparedRead work;
    CHECK(L::nextRead(c, now, work) && work.ready && work.transfer.index == 0x1001 &&
          work.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
    // A permission covers its exact subindex only.
    L::RepeatPermission wrong[] = {fault, reg};
    wrong[1].subindex = 1;
    out = R;
    CHECK(L::classifyRead(c, wrong, 2, out) && out == N);
    // An entry naming no field of this read is ignored, and changes nothing on an R read.
    static L::ReadContext probe;
    CHECK(L::prepareProbe(probe, t, 2, now, now + SPAN, options()) && probe.retryClass == R);
    out = N;
    CHECK(L::classifyRead(probe, both, 2, out) && out == R);
    out = N;
    CHECK(L::classifyRead(probe, nullptr, 0, out) && out == R);
    // Refusals leave the output as it was.
    out = R;
    L::RepeatPermission unrecorded = fault;
    unrecorded.record = 0;
    const L::RepeatPermission mixed[] = {reg, unrecorded};
    M::Status s = L::classifyRead(c, mixed, 2, out);
    CHECK(!s && s.code == M::Err::INVALID_CONFIG && s.detail == int32_t(M::ReadError::ARGUMENT));
    CHECK(out == R);
    s = L::classifyRead(c, nullptr, 1, out);
    CHECK(!s && s.detail == int32_t(M::ReadError::ARGUMENT) && out == R);
    static L::RepeatPermission many[L::REPEAT_PERMISSIONS + 1];
    for (L::RepeatPermission& p : many) {
        p = fault;
        p.record = 7;
    }
    CHECK(L::classifyRead(c, many, L::REPEAT_PERMISSIONS, out) && out == N); // Only 603F named.
    out = R;
    s = L::classifyRead(c, many, L::REPEAT_PERMISSIONS + 1, out);
    CHECK(!s && s.detail == int32_t(M::ReadError::ARGUMENT) && out == R);
    static L::ReadContext empty;
    s = L::classifyRead(empty, both, 2, out);
    CHECK(!s && s.detail == int32_t(M::ReadError::STATE) && out == R);
    // The output may not be the context's own field.
    const M::RetryClass own = c.retryClass;
    s = L::classifyRead(c, both, 2, c.retryClass);
    CHECK(!s && s.detail == int32_t(M::ReadError::OVERLAP));
    CHECK(c.retryClass == own && prepared.same(c));
    // A promoted class authorizes nothing here: the read still yields its unqualified uploads, and
    // after a terminal no work is offered again.
    CHECK(L::advanceRead(c, M::ReadOutcome::CANCELLED, now));
    CHECK(L::classifyRead(c, both, 2, out) && out == R);
    CHECK(L::nextRead(c, now, work) && !work.ready);
}

// Stop scenarios share one manufactured axis: an acknowledged 000F, then fresh state reads.
struct StopFixture {
    L::AxisReservation reservation;
    C::SdoCompletion control;
    L::ReadContext state;
    uint64_t now = 100;
    uint32_t operation = 1;
    StopFixture() {
        reservation.target = target();
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = 1;
        reservation.sequence = 1;
        reservation.generation = 1;
        reservation.acquiredUs = 0;
        control = acknowledge(0x000f);
    }
    L::ActionOptions policy() const {
        L::ActionOptions value;
        value.service = options();
        value.observationIntervalUs = 10;
        value.maximumAgeUs = 1000;
        return value;
    }
    // A caller-issued controlword that the drive acknowledged; it carries no stop history.
    C::SdoCompletion acknowledge(uint16_t value) {
        C::SdoTransfer t;
        t.channel = target().channel;
        t.operation = t.request = operation++;
        t.step = 1;
        t.index = 0x6040;
        t.size = 2;
        t.direction = C::SdoDirection::DOWNLOAD;
        t.data[0] = uint8_t(value);
        t.data[1] = uint8_t(value >> 8);
        t.deadlineUs = now + SPAN;
        t.queueTimeoutUs = t.responseTimeoutUs = 100;
        const C::SdoCompletion e = success(t, 0, now, now + 1);
        now += 2;
        return e;
    }
    // Fresh MONITORS and STATUS after everything so far, in mode 1.
    void read(uint16_t monitor, uint16_t status) {
        CHECK(L::prepareState(state, target(), operation++, now, now + SPAN, options(),
                              blocks(L::StateBlock::MONITORS, L::StateBlock::STATUS)));
        const uint32_t raws[] = {monitor, 0xa580, 0xf008, status, 1};
        complete(state, now, raws, 5);
    }
    // Yield, note and settle the action's next service, returning the yielded work.
    C::SdoTransfer serve(L::ActionContext& a, bool answered, uint32_t raw = 0) {
        L::PreparedAction work;
        CHECK(L::nextAction(a, reservation, target(), now, work) && work.ready);
        if (now < work.eligibleUs)
            now = work.eligibleUs;
        const bool download = work.transfer.direction == C::SdoDirection::DOWNLOAD;
        if (download)
            CHECK(L::noteActionTransmission(a, reservation, target(), now));
        const C::SdoCompletion e = answered ? success(work.transfer, raw, now, now + 1)
                                            : lost(work.transfer, now, now + 200);
        now = e.completedUs + 1;
        CHECK(L::advanceAction(a, reservation, target(), e, now));
        return work.transfer;
    }
};

// A quick stop is prepared again as a new operation, and each yields 6040=000B and nothing else.
void quickStopReissue() {
    static StopFixture f;
    f.read(L::MONITOR_RUNNING, 0x0037);
    static L::ActionContext first;
    const uint32_t firstOperation = f.operation++;
    CHECK(L::prepareStop(first, f.reservation, f.state, f.control, L::StopKind::QUICK_STOP,
                         firstOperation, f.now, f.now + 5000, f.policy()));
    CHECK(first.retryClass == N && first.state == M::ActionState::ACTIVE);
    // The command is lost after it may have left: execution unknown, and nothing is offered again.
    const C::SdoTransfer sent = f.serve(first, false);
    CHECK(controlword(sent, 0x000b) && sent.operation == firstOperation && sent.step == 1);
    CHECK(first.state == M::ActionState::FAILED);
    CHECK(first.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
    CHECK(first.execution == M::ActionExecution::UNKNOWN && first.uncertain);
    CHECK(first.retryClass == N);
    L::PreparedAction none;
    CHECK(L::nextAction(first, f.reservation, target(), f.now, none) && !none.ready);
    const Saved<L::ActionContext> settled(first);

    // The application's own new stop: a fresh state read still shows the drive switched on, and the
    // latest acknowledged word is still 000F, so a new QUICK_STOP is admitted under a new identity.
    // Keeping that 000F is this application's decision, which the library cannot check. AxisSession
    // withdraws it when a stop may have left and reissues through BestEffortStop instead
    // (bestEffortReissue); either way the new operation sends 000B only. A HALT is never prepared
    // this way after an unconfirmed 010F (ActionContext::retryClass).
    f.read(L::MONITOR_RUNNING, 0x0037);
    static L::ActionContext second;
    const uint32_t secondOperation = f.operation++;
    CHECK(L::prepareStop(second, f.reservation, f.state, f.control, L::StopKind::QUICK_STOP,
                         secondOperation, f.now, f.now + 5000, f.policy()));
    CHECK(second.retryClass == N && secondOperation != firstOperation);
    // The stop runs to its drive-reported terminal (released, low bits 000). Its only download is
    // 000B; everything after it is an observation upload.
    unsigned downloads = 0;
    const C::SdoTransfer command = f.serve(second, true);
    CHECK(controlword(command, 0x000b) && command.operation == secondOperation);
    CHECK(command.step == 1);
    ++downloads;
    CHECK(second.execution == M::ActionExecution::ACKNOWLEDGED);
    const uint32_t released[] = {L::MONITOR_RELEASED, 0xa580, 0xf008, 0x0050, 1};
    for (unsigned i = 0; i < 5 && second.state == M::ActionState::ACTIVE; ++i) {
        const C::SdoTransfer t = f.serve(second, true, released[i]);
        CHECK(t.direction == C::SdoDirection::UPLOAD && t.operation == secondOperation);
        downloads += t.direction == C::SdoDirection::DOWNLOAD;
    }
    CHECK(second.state == M::ActionState::SUCCEEDED);
    CHECK(second.outcome == M::ActionOutcome::OBSERVED);
    CHECK(downloads == 1 && second.commandEvidenceMask == 1);
    CHECK(L::nextAction(second, f.reservation, target(), f.now, none) && !none.ready);
    // The first stop's retained terminal is not rewritten by the second's success.
    CHECK(settled.same(first));

    // The acknowledged 000B is no stop predecessor: neither kind can be prepared from it.
    f.control = second.commandHistory[0];
    f.read(0, 0x0037);
    static L::ActionContext third;
    const Saved<L::ActionContext> before(third);
    CHECK(!L::prepareStop(third, f.reservation, f.state, f.control, L::StopKind::QUICK_STOP,
                          f.operation++, f.now, f.now + 5000, f.policy()));
    CHECK(!L::prepareStop(third, f.reservation, f.state, f.control, L::StopKind::HALT,
                          f.operation++, f.now, f.now + 5000, f.policy()));
    CHECK(before.same(third));
}

// HALT's 010F is never produced again by a preparation the existing rules refuse: after an
// acknowledged 010F a new HALT is refused, unchanged, while a QUICK_STOP is admitted and sends
// 000B.
void haltNotRepeated() {
    static StopFixture f;
    f.read(L::MONITOR_RUNNING, 0x0037);
    static L::ActionContext halt;
    CHECK(L::prepareStop(halt, f.reservation, f.state, f.control, L::StopKind::HALT, f.operation++,
                         f.now, f.now + 5000, f.policy()));
    CHECK(halt.retryClass == N);
    const C::SdoTransfer command = f.serve(halt, true);
    CHECK(controlword(command, 0x010f) && halt.execution == M::ActionExecution::ACKNOWLEDGED);
    const uint32_t idle[] = {0, 0xa580, 0xf008, 0x0037, 1};
    for (unsigned i = 0; i < 5 && halt.state == M::ActionState::ACTIVE; ++i)
        CHECK(f.serve(halt, true, idle[i]).direction == C::SdoDirection::UPLOAD);
    CHECK(halt.state == M::ActionState::SUCCEEDED);
    f.control = halt.commandHistory[0];
    CHECK(controlword(f.control.transfer, 0x010f));

    f.read(0, 0x0037);
    static L::ActionContext again;
    const Saved<L::ActionContext> empty(again);
    const M::Status refused = L::prepareStop(again, f.reservation, f.state, f.control,
                                             L::StopKind::HALT, f.operation++, f.now, f.now + 5000,
                                             f.policy());
    CHECK(!refused && refused.detail == int32_t(M::ActionError::STATE));
    CHECK(!L::validateStop(again, f.reservation, f.state, f.control, L::StopKind::HALT, f.operation,
                           f.now, f.now + 5000, f.policy()));
    CHECK(empty.same(again));
    L::PreparedAction none;
    CHECK(!L::nextAction(again, f.reservation, target(), f.now, none) && !none.ready);
    // The quick stop after 010F is the project's DEC-8 arrow: a new operation, 000B only.
    const uint32_t quick = f.operation++;
    CHECK(L::prepareStop(again, f.reservation, f.state, f.control, L::StopKind::QUICK_STOP, quick,
                         f.now, f.now + 5000, f.policy()));
    CHECK(again.retryClass == N && !again.predecessorDocumented && again.predecessorWord == 0x010f);
    L::PreparedAction work;
    CHECK(L::nextAction(again, f.reservation, target(), f.now, work) && work.ready);
    CHECK(controlword(work.transfer, 0x000b) && work.transfer.operation == quick);
}

// The best-effort stop has no history to consume: each new one is its own operation and yields
// exactly 6040=000B.
void bestEffortReissue() {
    const L::ReadTarget t = target();
    const uint64_t now = 100;
    static L::BestEffortStop a, b;
    CHECK(L::prepareBestEffortStop(a, t, 31, now, now + 5000, options()) && a.retryClass == N);
    CHECK(L::prepareBestEffortStop(b, t, 32, now, now + 5000, options()) && b.retryClass == N);
    L::PreparedAction first, second;
    CHECK(L::nextBestEffortStop(a, t, now, first) && first.ready);
    CHECK(L::nextBestEffortStop(b, t, now, second) && second.ready);
    CHECK(controlword(first.transfer, 0x000b) && controlword(second.transfer, 0x000b));
    CHECK(first.transfer.operation == 31 && second.transfer.operation == 32);
    // Only the identity differs: the same single word, step and budgets.
    CHECK(first.transfer.step == 1 && second.transfer.step == 1 &&
          !std::memcmp(first.transfer.data, second.transfer.data, sizeof first.transfer.data) &&
          first.transfer.deadlineUs == second.transfer.deadlineUs &&
          first.transfer.responseTimeoutUs == second.transfer.responseTimeoutUs);
    const Saved<L::BestEffortStop> fresh(a);
    CHECK(!L::prepareBestEffortStop(a, t, 0, now, now + 5000, options()));
    CHECK(fresh.same(a));
}
} // namespace

int main() {
    defaultsAreNotRepeatable();
    repeatableEffects();
    readClasses();
    refusedPreparationKeepsClass();
    permissions();
    quickStopReissue();
    haltNotRepeated();
    bestEffortReissue();
    std::printf("retry class tests passed (%u checks)\n", checks);
    return 0;
}
