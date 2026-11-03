// SPDX-License-Identifier: MIT
// Every terminal-consuming entry is walked field by field, and at each step every corruption of an
// otherwise valid terminal must be refused with the whole context byte-identical. The valid terminal
// is then accepted, which proves each refusal came from its corruption and not a broken fixture.
// All traffic and times are synthetic; nothing here qualifies a drive.
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
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
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool same(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};
// Status::msg is static text that a caller may print, so every refusal must carry one.
bool refused(const M::Status& status) {
    return !status && status.code != M::Err::OK && status.msg && *status.msg;
}

L::ReadTarget target() {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = 17;
    t.channel.binding.endpoint.profile = 41;
    t.channel.binding.endpoint.address = 5;
    t.channel.binding.endpoint.bus = 2;
    t.channel.binding.endpoint.channel = 1;
    t.channel.binding.endpoint.subaxis = 1;
    t.channel.binding.bindingGeneration = 3;
    t.channel.binding.configurationGeneration = 7;
    t.channel.clientToServerId = 0x625;
    t.channel.serverToClientId = 0x5a5;
    t.nodeGeneration = 11;
    return t;
}
L::ReadOptions options() {
    L::ReadOptions o;
    o.queueTimeoutUs = o.responseTimeoutUs = 100;
    return o;
}
const uint64_t ACQUIRED = 1000, DEADLINE = 900000;
L::AxisReservation reservation() {
    L::AxisReservation r;
    r.target = target();
    r.model = L::SettingsModel::CL86_C;
    r.owner = 3;
    r.sequence = 5;
    r.generation = 9;
    r.acquiredUs = ACQUIRED;
    return r;
}
uint8_t mask(std::initializer_list<L::StateBlock> blocks) {
    unsigned out = 0;
    for (L::StateBlock block : blocks)
        out |= 1u << unsigned(block);
    return uint8_t(out);
}

// A terminal as the sole owner would retain it, written out here rather than derived from the
// private helpers under test.
C::SdoCompletion acknowledged(const C::SdoTransfer& work, uint64_t admitted, uint64_t completed,
                              uint32_t raw = 0) {
    C::SdoCompletion e;
    e.transfer = work;
    e.outcome = C::SdoOutcome::SUCCESS;
    e.nodeGeneration = target().nodeGeneration;
    e.admittedUs = admitted;
    e.completedUs = completed;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = true;
    e.protocolAcknowledged = e.effectPossible = true;
    if (work.direction == C::SdoDirection::UPLOAD) {
        e.size = work.size;
        for (unsigned i = 0; i < e.size; ++i)
            e.data[i] = uint8_t(raw >> (8u * i));
    }
    return e;
}
C::SdoCompletion failed(const C::SdoTransfer& work, uint64_t admitted, uint64_t completed,
                        C::SdoOutcome outcome, bool possible) {
    C::SdoCompletion e;
    e.transfer = work;
    e.outcome = outcome;
    e.nodeGeneration = target().nodeGeneration;
    e.admittedUs = admitted;
    e.completedUs = completed;
    e.queued = e.transmissionPossible = e.effectPossible = e.effectUncertain = possible;
    if (outcome == C::SdoOutcome::SERVER_ABORT) {
        e.abortValid = true;
        e.abortCode = 0xf1234567u;
    }
    return e;
}

struct Times {
    uint64_t stepStart, deadline, now;
};
enum Applies : uint8_t { BOTH, UPLOAD_ONLY, DOWNLOAD_ONLY };
struct Mutant {
    const char* name;
    Applies applies;
    void (*apply)(C::SdoCompletion&, const Times&);
};
#define MUTANT(label, applies, ...)                                                                \
    {                                                                                              \
        label, applies, [](C::SdoCompletion& e, const Times& t) {                                  \
            (void)t;                                                                               \
            __VA_ARGS__;                                                                           \
        }                                                                                          \
    }
// One bad field of a transfer's route identity: every field of the channel and its binding.
#define ROUTE_MUTANTS                                                                              \
    MUTANT("logical axis", BOTH, ++e.transfer.channel.binding.logicalAxisId),                      \
        MUTANT("bus", BOTH, ++e.transfer.channel.binding.endpoint.bus),                            \
        MUTANT("profile", BOTH, ++e.transfer.channel.binding.endpoint.profile),                    \
        MUTANT("node address", BOTH, ++e.transfer.channel.binding.endpoint.address),               \
        MUTANT("endpoint channel", BOTH, ++e.transfer.channel.binding.endpoint.channel),           \
        MUTANT("subaxis", BOTH, ++e.transfer.channel.binding.endpoint.subaxis),                    \
        MUTANT("protocol", BOTH,                                                                   \
               e.transfer.channel.binding.endpoint.protocol = static_cast<M::Protocol>(200)),      \
        MUTANT("binding epoch", BOTH, ++e.transfer.channel.binding.bindingGeneration),             \
        MUTANT("older binding epoch", BOTH, --e.transfer.channel.binding.bindingGeneration),       \
        MUTANT("configuration epoch", BOTH, ++e.transfer.channel.binding.configurationGeneration), \
        MUTANT("older configuration epoch", BOTH,                                                  \
               --e.transfer.channel.binding.configurationGeneration),                              \
        MUTANT("client channel id", BOTH, ++e.transfer.channel.clientToServerId),                  \
        MUTANT("server channel id", BOTH, ++e.transfer.channel.serverToClientId)
// Everything else the admitted transfer states. Identical for every live-work entry.
#define WORK_MUTANTS                                                                               \
    MUTANT("operation", BOTH, ++e.transfer.operation),                                             \
        MUTANT("request", BOTH, ++e.transfer.request), MUTANT("step", BOTH, ++e.transfer.step),    \
        MUTANT("earlier step", BOTH, --e.transfer.step),                                           \
        MUTANT("object", BOTH, ++e.transfer.index),                                                \
        MUTANT("subindex", BOTH, ++e.transfer.subindex),                                           \
        MUTANT("wider", BOTH, ++e.transfer.size), MUTANT("narrower", BOTH, --e.transfer.size),     \
        MUTANT("direction", BOTH,                                                                  \
               e.transfer.direction = e.transfer.direction == C::SdoDirection::UPLOAD              \
                                          ? C::SdoDirection::DOWNLOAD                              \
                                          : C::SdoDirection::UPLOAD),                              \
        MUTANT("command byte 0", DOWNLOAD_ONLY, e.transfer.data[0] ^= 1),                          \
        MUTANT("command byte 1", DOWNLOAD_ONLY, e.transfer.data[1] ^= 0x80),                       \
        MUTANT("deadline later", BOTH, ++e.transfer.deadlineUs),                                   \
        MUTANT("deadline earlier", BOTH, --e.transfer.deadlineUs),                                 \
        MUTANT("queue budget", BOTH, ++e.transfer.queueTimeoutUs),                                 \
        MUTANT("response budget", BOTH, ++e.transfer.responseTimeoutUs),                           \
        MUTANT("upload size mode", BOTH,                                                           \
               e.transfer.uploadSize = C::SdoUploadSize::SERVER_INDICATED),                        \
        MUTANT("read effect", BOTH, e.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING), \
        MUTANT("segmented permission", BOTH, e.transfer.allowSegmentedUpload = false),             \
        MUTANT("pre-operational premise", BOTH, e.transfer.requirePreOperational = true),          \
        MUTANT("later node epoch", BOTH, ++e.nodeGeneration),                                      \
        MUTANT("older node epoch", BOTH, --e.nodeGeneration),                                      \
        MUTANT("no node anchor", BOTH, e.nodeGeneration = 0),                                      \
        MUTANT("admitted before step", BOTH, e.admittedUs = t.stepStart - 1),                      \
        MUTANT("admitted at cutoff", BOTH, e.admittedUs = e.completedUs = t.deadline),             \
        MUTANT("completed before admitted", BOTH, e.admittedUs = e.completedUs + 1),               \
        MUTANT("completed in the future", BOTH, e.completedUs = t.now + 1)
const Mutant successMutants[] = {
    ROUTE_MUTANTS,
    WORK_MUTANTS,
    MUTANT("no bytes", UPLOAD_ONLY, e.size = 0),
    MUTANT("short reply", UPLOAD_ONLY, --e.size),
    MUTANT("long reply", UPLOAD_ONLY, ++e.size),
    MUTANT("download returned bytes", DOWNLOAD_ONLY, e.size = 1),
    MUTANT("not queued", BOTH, e.queued = false),
    MUTANT("success never possible", BOTH,
           e.transmissionPossible = e.transmissionConfirmed = false),
    MUTANT("confirmed never possible", BOTH, e.transmissionPossible = false),
    MUTANT("no acknowledgement", BOTH, e.protocolAcknowledged = false),
    MUTANT("no effect possible", BOTH, e.effectPossible = false),
    MUTANT("uncertain success", BOTH, e.effectUncertain = true),
    MUTANT("success with abort", BOTH, e.abortValid = true),
    MUTANT("best-effort outcome", BOTH, e.outcome = C::SdoOutcome::UNCONFIRMED_SENT),
    MUTANT("unknown outcome", BOTH, e.outcome = static_cast<C::SdoOutcome>(200)),
};
// Contradictions of a timeout that may have reached the wire, and of a server abort.
const Mutant timeoutMutants[] = {
    MUTANT("not queued", BOTH, e.queued = false),
    MUTANT("no effect possible", BOTH, e.effectPossible = false),
    MUTANT("settled effect", BOTH, e.effectUncertain = false),
    MUTANT("acknowledged timeout", BOTH, e.protocolAcknowledged = true),
    MUTANT("timeout with bytes", BOTH, e.size = 1),
    MUTANT("no possible transmission", BOTH, e.transmissionPossible = false),
    MUTANT("best-effort outcome", BOTH, e.outcome = C::SdoOutcome::UNCONFIRMED_SENT),
    MUTANT("unknown outcome", BOTH, e.outcome = static_cast<C::SdoOutcome>(200)),
};
// Contradictions of a timeout that never left the host.
const Mutant unsentMutants[] = {
    MUTANT("confirmed never possible", BOTH, e.transmissionConfirmed = true),
    MUTANT("effect without transmission", BOTH, e.effectPossible = true),
    MUTANT("uncertainty without transmission", BOTH, e.effectUncertain = true),
    MUTANT("unsent timeout with bytes", BOTH, e.size = 1),
    MUTANT("unsent timeout acknowledged", BOTH, e.protocolAcknowledged = true),
};
const Mutant abortMutants[] = {
    MUTANT("abort without code", BOTH, e.abortValid = false),
    MUTANT("abort never possible", BOTH,
           e.transmissionPossible = e.effectPossible = e.effectUncertain = false),
    MUTANT("abort with bytes", BOTH, e.size = 1),
};

bool applicable(const Mutant& m, const C::SdoTransfer& work) {
    return m.applies == BOTH ||
           (m.applies == UPLOAD_ONLY) == (work.direction == C::SdoDirection::UPLOAD);
}
// Mutants are refused against the unchanged context. The caller's Advance closes over its own
// live reservation and binding, which do not change here.
template <class Context, class Advance, size_t N>
void refuseEach(const Mutant (&mutants)[N], Context& c, const C::SdoCompletion& base,
                const Times& t, Advance advance, const char* where) {
    const Saved<Context> before(c);
    for (const Mutant& m : mutants) {
        if (!applicable(m, base.transfer))
            continue;
        C::SdoCompletion bad = base;
        m.apply(bad, t);
        const M::Status status = advance(c, bad, t.now);
        ++checks;
        if (!refused(status) || !before.same(c)) {
            std::fprintf(stderr, "%s: mutant '%s' at %04x:%02x: %s\n", where, m.name,
                         base.transfer.index, base.transfer.subindex,
                         refused(status) ? "changed the context" : "was accepted");
            std::abort();
        }
    }
}
// Everything an ordinary service step must refuse, then the valid terminal. Times leave a gap on
// each side of the terminal so that each timing mutant breaks only its own bound.
template <class Context, class Advance>
void sweepStep(Context& c, const C::SdoTransfer& work, uint64_t stepStart, uint64_t& now,
               uint64_t deadline, uint32_t raw, bool possibleTimeoutConsumable, Advance advance,
               const char* where) {
    const uint64_t admitted = now + 2, completed = now + 5;
    now += 10;
    const Times t = {stepStart, deadline, now};
    const C::SdoCompletion valid = acknowledged(work, admitted, completed, raw);
    refuseEach(successMutants, c, valid, t, advance, where);
    if (possibleTimeoutConsumable) {
        const C::SdoCompletion timeout =
            failed(work, admitted, completed, C::SdoOutcome::TIMEOUT, true);
        refuseEach(timeoutMutants, c, timeout, t, advance, where);
        const C::SdoCompletion abort =
            failed(work, admitted, completed, C::SdoOutcome::SERVER_ABORT, true);
        refuseEach(abortMutants, c, abort, t, advance, where);
        const C::SdoCompletion unsent =
            failed(work, admitted, completed, C::SdoOutcome::TIMEOUT, false);
        refuseEach(unsentMutants, c, unsent, t, advance, where);
    }
    CHECK(advance(c, valid, now));
}

uint32_t fieldValue(uint16_t index, uint8_t subindex) {
    return index == 0x1003 && subindex == 0 ? 4 : 1; // A full history visits every entry.
}
const auto advanceReadCall = [](L::ReadContext& c, const C::SdoCompletion& e, uint64_t now) {
    return L::advanceRead(c, e, now);
};
const auto advanceDriverCall = [](L::DriverContext& c, const C::SdoCompletion& e, uint64_t now) {
    return L::advanceDriver(c, e, now);
};

// Walk a read through every planned field.
uint64_t walkRead(L::ReadContext& c, uint64_t now, const char* where) {
    while (c.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(c, now, work) && work.ready);
        sweepStep(c, work.transfer, c.servicedUs, now, c.deadlineUs,
                  fieldValue(work.transfer.index, work.transfer.subindex), true, advanceReadCall,
                  where);
    }
    return now;
}
void readEntries() {
    uint64_t now = ACQUIRED;
    L::ReadContext c;
    CHECK(L::prepareProbe(c, target(), 1, now, DEADLINE, options()));
    now = walkRead(c, now, "probe");
    CHECK(L::prepareIdentity(c, target(), 2, now, DEADLINE, options()));
    now = walkRead(c, now, "identity");
    L::IdentityObservation identity;
    CHECK(L::getIdentity(c, target(), now, 1000000, identity));
    CHECK(L::prepareConfig(c, target(), 3, now, DEADLINE, options()));
    now = walkRead(c, now, "config");
    L::ConfigObservation config;
    CHECK(L::getConfig(c, target(), now, 1000000, config));
    for (unsigned selected = 1; selected < 64; ++selected) {
        CHECK(L::prepareState(c, target(), 4, now, DEADLINE, options(), uint8_t(selected)));
        now = walkRead(c, now, "state");
    }
}
void driverEntries() {
    uint64_t now = ACQUIRED;
    const L::DriverGroup groups[] = {
        L::DriverGroup::DRIVER,
        L::DriverGroup::SOFT_LIMITS,
        L::DriverGroup::MOTION_PARAMETERS,
        L::DriverGroup::IO_INPUT_CONFIGURATION,
        L::DriverGroup::IO_OUTPUT_CONFIGURATION,
        L::DriverGroup::IO_INPUT_FILTERS,
        L::DriverGroup::IO_OUTPUT_COMMAND,
        L::DriverGroup::CONTROL,
        L::DriverGroup::TUNING_CURRENT_HOLDING,
        L::DriverGroup::TUNING_FILTER_POSITION,
        L::DriverGroup::TUNING_LOOP_GAINS,
        L::DriverGroup::TUNING_EDS_GAINS,
    };
    const auto walk = [&now](L::DriverContext& c, const char* where) {
        while (c.read.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextDriver(c, now, work) && work.ready);
            sweepStep(c, work.transfer, c.read.servicedUs, now, c.read.deadlineUs,
                      fieldValue(work.transfer.index, work.transfer.subindex), true,
                      advanceDriverCall, where);
        }
    };
    L::DriverContext c;
    for (L::DriverGroup group : groups) {
        CHECK(L::prepareDriverRead(c, target(), 5, now, DEADLINE, options(), group));
        walk(c, "driver read");
    }
    CHECK(L::prepareControlRead(c, target(), 6, now, DEADLINE, options()));
    walk(c, "control read");
    L::ControlObservation control;
    CHECK(L::getControl(c, target(), now, 1000000, control));
    const L::TuningGroup tuning[] = {L::TuningGroup::CURRENT_HOLDING, L::TuningGroup::FILTER_POSITION,
                                     L::TuningGroup::LOOP_GAINS, L::TuningGroup::EDS_GAINS};
    for (L::TuningGroup group : tuning) {
        CHECK(L::prepareTuningRead(c, target(), 7, now, DEADLINE, options(), group));
        walk(c, "tuning read");
        L::TuningObservation observed;
        CHECK(L::getTuning(c, target(), now, 1000000, observed));
    }
}

// One wrong field of a caller-supplied current target.
const unsigned TARGET_MUTANTS = 12;
void mutateTarget(L::ReadTarget& t, unsigned which) {
    M::Binding& b = t.channel.binding;
    switch (which) {
    case 0: ++b.logicalAxisId; break;
    case 1: ++b.endpoint.bus; break;
    case 2: ++b.endpoint.profile; break;
    case 3: ++b.endpoint.address; break;
    case 4: ++b.endpoint.channel; break;
    case 5: ++b.endpoint.subaxis; break;
    case 6: ++b.bindingGeneration; break;
    case 7: ++b.configurationGeneration; break;
    case 8: --b.configurationGeneration; break;
    case 9: ++t.channel.clientToServerId; break;
    case 10: ++t.channel.serverToClientId; break;
    default: ++t.nodeGeneration; break;
    }
}
// One wrong field of the caller-supplied live reservation, including its frozen route.
const unsigned RESERVATION_MUTANTS = TARGET_MUTANTS + 6;
void mutateReservation(L::AxisReservation& r, unsigned which) {
    if (which < TARGET_MUTANTS)
        return mutateTarget(r.target, which);
    switch (which - TARGET_MUTANTS) {
    case 0: r.model = L::SettingsModel::OL86_C; break;
    case 1: r.model = L::SettingsModel::UNKNOWN; break;
    case 2: ++r.owner; break;
    case 3: ++r.sequence; break;
    case 4: ++r.generation; break;
    default: ++r.acquiredUs; break;
    }
}

// Every checked getter of a finished read, against every wrong current target.
template <class Output, class Get> void gettersRefuseWrongTargets(const L::ReadTarget& good, Get get) {
    Output output;
    const Saved<Output> blank(output);
    for (unsigned which = 0; which < TARGET_MUTANTS; ++which) {
        L::ReadTarget wrong = good;
        mutateTarget(wrong, which);
        const M::Status status = get(wrong, output);
        CHECK(refused(status) && status.detail == int32_t(M::ReadError::TARGET));
        CHECK(blank.same(output));
    }
    CHECK(get(good, output));
}
void getterTargets() {
    uint64_t now = ACQUIRED;
    const auto finishedRead = [&now](L::ReadContext& c) {
        while (c.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(c, now, work) && work.ready);
            ++now;
            CHECK(L::advanceRead(c, acknowledged(work.transfer, now, now, 1), now));
        }
    };
    const uint64_t age = 1000000;
    L::ReadContext read;
    CHECK(L::prepareIdentity(read, target(), 1, now, DEADLINE, options()));
    finishedRead(read);
    gettersRefuseWrongTargets<L::IdentityObservation>(
        target(), [&](const L::ReadTarget& t, L::IdentityObservation& out) {
            return L::getIdentity(read, t, now, age, out);
        });
    CHECK(L::prepareConfig(read, target(), 1, now, DEADLINE, options()));
    finishedRead(read);
    gettersRefuseWrongTargets<L::ConfigObservation>(
        target(), [&](const L::ReadTarget& t, L::ConfigObservation& out) {
            return L::getConfig(read, t, now, age, out);
        });
    CHECK(L::prepareState(read, target(), 1, now, DEADLINE, options(),
                          mask({L::StateBlock::STATUS, L::StateBlock::MONITORS})));
    finishedRead(read);
    gettersRefuseWrongTargets<L::StateObservation>(
        target(), [&](const L::ReadTarget& t, L::StateObservation& out) {
            return L::getStateBlock(read, L::StateBlock::STATUS, t, now, age, out);
        });
    gettersRefuseWrongTargets<L::StateInterpretation>(
        target(), [&](const L::ReadTarget& t, L::StateInterpretation& out) {
            return L::getStateInterpretation(read, t, now, age, L::StatusLayout::EN86, out);
        });
    gettersRefuseWrongTargets<L::ReadWindow>(target(), [&](const L::ReadTarget& t, L::ReadWindow& out) {
        return L::validateStateBlock(read, L::StateBlock::MONITORS, t, now, age, out);
    });
    L::DriverContext group;
    CHECK(L::prepareDriverRead(group, target(), 1, now, DEADLINE, options(),
                               L::DriverGroup::SOFT_LIMITS));
    finishedRead(group.read);
    gettersRefuseWrongTargets<L::DriverObservation>(
        target(), [&](const L::ReadTarget& t, L::DriverObservation& out) {
            return L::getDriver(group, t, now, age, out);
        });
    CHECK(L::prepareDriverRead(group, target(), 1, now, DEADLINE, options(),
                               L::DriverGroup::IO_INPUT_FILTERS));
    finishedRead(group.read);
    gettersRefuseWrongTargets<L::IoObservation>(
        target(), [&](const L::ReadTarget& t, L::IoObservation& out) {
            return L::getIo(group, t, now, age, out);
        });
    CHECK(L::prepareControlRead(group, target(), 1, now, DEADLINE, options()));
    finishedRead(group.read);
    gettersRefuseWrongTargets<L::ControlObservation>(
        target(), [&](const L::ReadTarget& t, L::ControlObservation& out) {
            return L::getControl(group, t, now, age, out);
        });
    CHECK(L::prepareTuningRead(group, target(), 1, now, DEADLINE, options(),
                               L::TuningGroup::LOOP_GAINS));
    finishedRead(group.read);
    gettersRefuseWrongTargets<L::TuningObservation>(
        target(), [&](const L::ReadTarget& t, L::TuningObservation& out) {
            return L::getTuning(group, t, now, age, out);
        });
}

// A finished read of one kind publishes through its own typed getter and no other.
void gettersRefuseOtherKinds() {
    uint64_t now = ACQUIRED;
    const L::DriverGroup groups[] = {
        L::DriverGroup::DRIVER, L::DriverGroup::SOFT_LIMITS, L::DriverGroup::MOTION_PARAMETERS,
        L::DriverGroup::IO_INPUT_CONFIGURATION, L::DriverGroup::IO_OUTPUT_CONFIGURATION,
        L::DriverGroup::IO_INPUT_FILTERS, L::DriverGroup::IO_OUTPUT_COMMAND, L::DriverGroup::CONTROL,
        L::DriverGroup::TUNING_CURRENT_HOLDING, L::DriverGroup::TUNING_FILTER_POSITION,
        L::DriverGroup::TUNING_LOOP_GAINS, L::DriverGroup::TUNING_EDS_GAINS};
    const auto finish = [&now](L::ReadContext& c) {
        while (c.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(c, now, work) && work.ready);
            ++now;
            CHECK(L::advanceRead(c, acknowledged(work.transfer, now, now, 1), now));
        }
    };
    const unsigned KINDS = 15;
    for (unsigned kind = 0; kind < KINDS; ++kind) {
        L::DriverContext c;
        if (kind < 12)
            CHECK(L::prepareDriverRead(c, target(), 1, now, DEADLINE, options(), groups[kind]));
        else if (kind == 12)
            CHECK(L::prepareIdentity(c.read, target(), 1, now, DEADLINE, options()));
        else if (kind == 13)
            CHECK(L::prepareConfig(c.read, target(), 1, now, DEADLINE, options()));
        else
            CHECK(L::prepareState(c.read, target(), 1, now, DEADLINE, options(),
                                  mask({L::StateBlock::STATUS})));
        finish(c.read);
        const Saved<L::DriverContext> kept(c);
        const uint64_t age = 1000000;
        L::DriverObservation driver;
        L::IoObservation io;
        L::ControlObservation control;
        L::TuningObservation tuning;
        L::IdentityObservation identity;
        L::ConfigObservation config;
        const Saved<L::DriverObservation> blankDriver(driver);
        const Saved<L::IoObservation> blankIo(io);
        const Saved<L::ControlObservation> blankControl(control);
        const Saved<L::TuningObservation> blankTuning(tuning);
        const Saved<L::IdentityObservation> blankIdentity(identity);
        const Saved<L::ConfigObservation> blankConfig(config);
        const M::Status a = L::getDriver(c, target(), now, age, driver);
        const M::Status b = L::getIo(c, target(), now, age, io);
        const M::Status d = L::getControl(c, target(), now, age, control);
        const M::Status e = L::getTuning(c, target(), now, age, tuning);
        const M::Status f = L::getIdentity(c.read, target(), now, age, identity);
        const M::Status g = L::getConfig(c.read, target(), now, age, config);
        CHECK(a.isOk() == (kind < 3) && b.isOk() == (kind >= 3 && kind < 7));
        CHECK(d.isOk() == (kind == 7) && e.isOk() == (kind >= 8 && kind < 12));
        CHECK(f.isOk() == (kind == 12) && g.isOk() == (kind == 13));
        CHECK(a.isOk() || (refused(a) && blankDriver.same(driver)));
        CHECK(b.isOk() || (refused(b) && blankIo.same(io)));
        CHECK(d.isOk() || (refused(d) && blankControl.same(control)));
        CHECK(e.isOk() || (refused(e) && blankTuning.same(tuning)));
        CHECK(f.isOk() || (refused(f) && blankIdentity.same(identity)));
        CHECK(g.isOk() || (refused(g) && blankConfig.same(config)));
        CHECK(kept.same(c));
    }
}

// A retained read with a chosen raw value per object, as a prerequisite for settings and actions.
struct Answers {
    uint32_t monitor = 0, statusword = 7, mode = 1, errorCode = 0;
};
void finishRead(L::ReadContext& c, uint64_t& now, const Answers& answers) {
    while (c.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(c, now, work) && work.ready);
        ++now;
        uint32_t raw = fieldValue(work.transfer.index, work.transfer.subindex);
        switch (work.transfer.index) {
        case 0x2100: raw = answers.monitor; break;
        case 0x6041: raw = answers.statusword; break;
        case 0x6061: raw = answers.mode; break;
        case 0x603f: raw = answers.errorCode; break;
        case 0x606c: raw = 0; break; // A counter clear admits only 606C zero.
        default: break;
        }
        CHECK(L::advanceRead(c, acknowledged(work.transfer, now, now, raw), now));
    }
    CHECK(c.state == M::ReadState::SUCCEEDED);
}
// A 6040:00 acknowledgement, as the application's history would keep it.
C::SdoCompletion controlWord(uint16_t word, uint64_t& now, uint64_t deadline = DEADLINE) {
    C::SdoTransfer work;
    work.channel = target().channel;
    work.operation = work.request = 31;
    work.step = 1;
    work.index = 0x6040;
    work.direction = C::SdoDirection::DOWNLOAD;
    work.size = 2;
    work.data[0] = uint8_t(word);
    work.data[1] = uint8_t(word >> 8);
    work.deadlineUs = deadline;
    work.queueTimeoutUs = work.responseTimeoutUs = 100;
    ++now;
    const uint64_t admitted = now;
    ++now;
    return acknowledged(work, admitted, now);
}

enum class SettingsKind { MOTION, IO, CONTROL };
// Runs one transaction through all its writes and readback, sweeping every step and every
// caller-supplied argument. All candidates differ from the prior read, so every field is written.
void settingsEntry(SettingsKind kind, const char* where) {
    const L::AxisReservation live = reservation();
    M::Binding binding = live.target.channel.binding;
    uint64_t now = ACQUIRED;
    C::SdoCompletion control = controlWord(0x000f, now);
    L::ReadContext state;
    CHECK(L::prepareState(state, target(), 8, now, DEADLINE, options(),
                          mask({L::StateBlock::STATUS, L::StateBlock::MONITORS})));
    finishRead(state, now, Answers());
    L::DriverContext prior;
    const L::DriverGroup group = kind == SettingsKind::MOTION ? L::DriverGroup::MOTION_PARAMETERS
                                 : kind == SettingsKind::IO   ? L::DriverGroup::IO_INPUT_CONFIGURATION
                                                              : L::DriverGroup::CONTROL;
    CHECK(L::prepareDriverRead(prior, target(), 9, now, DEADLINE, options(), group));
    finishRead(prior.read, now, Answers());

    L::DriverSettingsContext c;
    if (kind == SettingsKind::MOTION) {
        L::DriverSettings candidate;
        candidate.positionSpeedRpm = 101;
        candidate.rampUpTimeMs = 201;
        candidate.rampDownTimeMs = 301;
        CHECK(L::prepareDriverSettings(c, live, live.target, prior, state, control, candidate, 41, now, DEADLINE,
                                       options(), 100000));
    } else if (kind == SettingsKind::IO) {
        L::IoSettings candidate;
        candidate.invertedInputs = 0x55;
        for (L::InputFunction& function : candidate.functions)
            function = L::InputFunction::STOP;
        CHECK(L::prepareIoSettings(c, live, live.target, prior, state, candidate, 41, now, DEADLINE, options(),
                                   100000));
    } else {
        L::ControlSettings candidate;
        candidate.loopMode = L::LoopMode::CLOSED_LOOP;
        CHECK(L::prepareControlSettings(c, live, live.target, prior, state, candidate, 41, now, DEADLINE, options(),
                                        100000));
    }
    CHECK(c.state == L::SettingsState::WRITING);

    unsigned steps = 0;
    while (c.state == L::SettingsState::WRITING || c.state == L::SettingsState::READING_BACK) {
        ++steps;
        const auto advance = [&](L::DriverSettingsContext& context, const C::SdoCompletion& e,
                                 uint64_t at) {
            return L::advanceDriver(context, live, binding, e, at);
        };
        L::PreparedRead work;
        CHECK(L::nextDriver(c, live, binding, now, work) && work.ready);
        const uint64_t stepStart = c.stepStartedUs;
        const bool writing = c.state == L::SettingsState::WRITING;
        CHECK(work.transfer.direction ==
              (writing ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD));

        // Every wrong current argument, on each of next, notice and advance.
        const Saved<L::DriverSettingsContext> before(c);
        const C::SdoCompletion probe = acknowledged(work.transfer, now, now);
        for (unsigned which = 0; which < RESERVATION_MUTANTS; ++which) {
            L::AxisReservation wrong = live;
            mutateReservation(wrong, which);
            L::PreparedRead output;
            output.transfer.index = 0xbeef;
            const Saved<L::PreparedRead> blank(output);
            CHECK(refused(L::nextDriver(c, wrong, binding, now, output)) && blank.same(output));
            M::Binding kept = binding;
            CHECK(refused(L::noteDriverTransmission(c, wrong, kept, now)) && M::sameBinding(kept, binding));
            CHECK(refused(L::advanceDriver(c, wrong, binding, probe, now)));
            CHECK(before.same(c));
        }
        for (unsigned which = 0; which < 9; ++which) {
            L::ReadTarget wrong = target();
            wrong.channel.binding = binding;
            mutateTarget(wrong, which);
            L::PreparedRead output;
            output.transfer.index = 0xbeef;
            const Saved<L::PreparedRead> blank(output);
            CHECK(refused(L::nextDriver(c, live, wrong.channel.binding, now, output)) && blank.same(output));
            M::Binding kept = wrong.channel.binding;
            CHECK(refused(L::noteDriverTransmission(c, live, kept, now)) &&
                  M::sameBinding(kept, wrong.channel.binding));
            CHECK(refused(L::advanceDriver(c, live, wrong.channel.binding, probe, now)));
            CHECK(before.same(c));
        }
        if (writing) {
            // A possible download needs the possible-TX notice first, whatever else matches.
            ++now;
            CHECK(refused(advance(c, acknowledged(work.transfer, now, now), now)));
            CHECK(before.same(c));
            CHECK(L::noteDriverTransmission(c, live, binding, now));
        }
        const unsigned field = writing ? c.cursor : c.readback.read.cursor;
        sweepStep(c, work.transfer, stepStart, now, DEADLINE, c.requested[field], true, advance, where);
    }
    CHECK(c.state == L::SettingsState::SUCCEEDED && steps >= 3);
    L::ReadTarget finalTarget = target();
    finalTarget.channel.binding = binding;
    CHECK(binding.configurationGeneration == 8);
    if (kind == SettingsKind::MOTION) {
        L::DriverObservation out;
        CHECK(L::getDriver(c, live, finalTarget, now, 1000000, out) && out.positionSpeedRpm == 101);
    } else if (kind == SettingsKind::IO) {
        L::IoObservation out;
        CHECK(L::getIo(c, live, finalTarget, now, 1000000, out));
    } else {
        L::ControlObservation out;
        CHECK(L::getControl(c, live, finalTarget, now, 1000000, out) &&
              out.loopMode == L::LoopMode::CLOSED_LOOP);
    }
}

L::ActionOptions actionOptions() {
    L::ActionOptions o;
    o.service = options();
    o.observationIntervalUs = 10;
    o.maximumAgeUs = 100000;
    return o;
}
M::AxisConfig axisFor(const L::ReadTarget& t) {
    M::AxisConfig axis;
    axis.binding = t.channel.binding;
    return axis;
}
// Reports a stop's cycle as running first, so the walk also crosses WAITING. Cycle answers are
// chosen so the action completes on its second cycle.
uint32_t actionAnswer(const L::ActionContext& c, uint16_t index, uint8_t subindex) {
    if (index == 0x2100)
        return c.actionKind == M::ActionKind::STOP && !c.observations ? L::MONITOR_RUNNING : 0;
    if (index == 0x6041)
        return 7;
    if (index == 0x6061)
        return 1;
    return index == 0x6064 || index == 0x606c ? 0 : fieldValue(index, subindex);
}
void actionEntry(M::ActionKind kind, L::StopKind stop, const char* where) {
    const L::AxisReservation live = reservation();
    L::ReadTarget current = target();
    M::AxisConfig axis = axisFor(current);
    M::AxisReference reference;
    uint64_t now = ACQUIRED;
    L::ActionContext c;
    Answers answers;
    if (kind == M::ActionKind::CLEAR_ALARM) {
        answers.monitor = L::MONITOR_ALARM;
        answers.errorCode = 0xff05;
    }
    const C::SdoCompletion control = controlWord(0x000f, now);
    L::ReadContext state;
    CHECK(L::prepareState(state, target(), 8, now, DEADLINE, options(),
                          kind == M::ActionKind::CLEAR_ALARM
                              ? mask({L::StateBlock::ERRORS, L::StateBlock::ERROR_HISTORY,
                                      L::StateBlock::MONITORS, L::StateBlock::STATUS})
                          : kind == M::ActionKind::SET_DEVICE_POSITION
                              ? mask({L::StateBlock::MONITORS, L::StateBlock::STATUS, L::StateBlock::FEEDBACK})
                              : mask({L::StateBlock::MONITORS, L::StateBlock::STATUS})));
    finishRead(state, now, answers);
    if (kind == M::ActionKind::STOP)
        CHECK(L::prepareStop(c, live, state, control, stop, 41, now, DEADLINE, actionOptions()));
    else if (kind == M::ActionKind::CLEAR_ALARM)
        CHECK(L::prepareClearAlarm(c, live, state, 41, now, DEADLINE, actionOptions()));
    else
        CHECK(L::prepareSetDevicePosition(c, live, state, 0, axis, 41, now, DEADLINE, actionOptions()));

    unsigned commands = 0, reads = 0;
    while (c.state == M::ActionState::ACTIVE) {
        if (now < c.eligibleUs)
            now = c.eligibleUs;
        L::PreparedAction work;
        CHECK(L::nextAction(c, live, current, now, work) && work.ready);
        const bool command = c.phase == L::ActionPhase::COMMAND;
        const uint64_t stepStart = c.stepStartedUs;
        const auto advance = [&](L::ActionContext& context, const C::SdoCompletion& e, uint64_t at) {
            return L::advanceAction(context, live, current, e, at);
        };
        const Saved<L::ActionContext> before(c);
        const C::SdoCompletion probe = acknowledged(work.transfer, now, now);
        for (unsigned which = 0; which < RESERVATION_MUTANTS; ++which) {
            L::AxisReservation wrong = live;
            mutateReservation(wrong, which);
            L::PreparedAction output;
            output.eligibleUs = 12345;
            const Saved<L::PreparedAction> blank(output);
            CHECK(refused(L::nextAction(c, wrong, current, now, output)) && blank.same(output));
            CHECK(refused(L::advanceAction(c, wrong, current, probe, now)));
            CHECK(before.same(c));
        }
        for (unsigned which = 0; which < TARGET_MUTANTS; ++which) {
            L::ReadTarget wrong = current;
            mutateTarget(wrong, which);
            L::PreparedAction output;
            output.eligibleUs = 12345;
            const Saved<L::PreparedAction> blank(output);
            CHECK(refused(L::nextAction(c, live, wrong, now, output)) && blank.same(output));
            CHECK(refused(L::advanceAction(c, live, wrong, probe, now)));
            CHECK(before.same(c));
        }
        if (command) {
            ++commands;
            ++now;
            CHECK(refused(advance(c, acknowledged(work.transfer, now, now), now)));
            CHECK(before.same(c));
            if (kind == M::ActionKind::SET_DEVICE_POSITION)
                CHECK(L::noteActionTransmission(c, live, current, axis, reference, now));
            else
                CHECK(L::noteActionTransmission(c, live, current, now));
        } else {
            ++reads;
        }
        sweepStep(c, work.transfer, stepStart, now, DEADLINE,
                  command ? 0 : actionAnswer(c, work.transfer.index, work.transfer.subindex), true,
                  advance, where);
    }
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(commands == (kind == M::ActionKind::STOP ? 1u : kind == M::ActionKind::CLEAR_ALARM ? 2u : 3u));
    CHECK(reads >= 5);
}

// A best-effort stop's only terminal: the frame left, and nothing can confirm it.
const Mutant bestEffortMutants[] = {
    ROUTE_MUTANTS,
    MUTANT("operation", BOTH, ++e.transfer.operation),
    MUTANT("object", BOTH, ++e.transfer.index),
    MUTANT("subindex", BOTH, ++e.transfer.subindex),
    MUTANT("width", BOTH, ++e.transfer.size),
    MUTANT("direction", BOTH, e.transfer.direction = C::SdoDirection::UPLOAD),
    MUTANT("command byte 0", BOTH, e.transfer.data[0] ^= 1),
    MUTANT("command byte 1", BOTH, e.transfer.data[1] ^= 0x80),
    MUTANT("deadline later", BOTH, ++e.transfer.deadlineUs),
    MUTANT("deadline earlier", BOTH, --e.transfer.deadlineUs),
    MUTANT("admitted before start", BOTH, e.admittedUs = t.stepStart - 1),
    MUTANT("completed before admitted", BOTH, e.admittedUs = e.completedUs + 1),
    MUTANT("completed in the future", BOTH, e.completedUs = t.now + 1),
    MUTANT("returned bytes", BOTH, e.size = 1),
    MUTANT("abort", BOTH, e.abortValid = true),
    MUTANT("acknowledged", BOTH, e.protocolAcknowledged = true),
    MUTANT("success", BOTH, e.outcome = C::SdoOutcome::SUCCESS),
    MUTANT("server abort", BOTH, e.outcome = C::SdoOutcome::SERVER_ABORT),
    MUTANT("unsupported", BOTH, e.outcome = C::SdoOutcome::UNSUPPORTED),
    MUTANT("unknown outcome", BOTH, e.outcome = static_cast<C::SdoOutcome>(200)),
    MUTANT("sent unconfirmed", BOTH, e.transmissionConfirmed = false),
    MUTANT("sent never possible", BOTH, e.transmissionPossible = false),
    MUTANT("no effect possible", BOTH, e.effectPossible = false),
    MUTANT("step", BOTH, ++e.transfer.step),
    MUTANT("request", BOTH, ++e.transfer.request),
    MUTANT("queue budget", BOTH, ++e.transfer.queueTimeoutUs),
    MUTANT("response budget", BOTH, ++e.transfer.responseTimeoutUs),
    MUTANT("node epoch later", BOTH, ++e.nodeGeneration),
    MUTANT("no node epoch", BOTH, e.nodeGeneration = 0),
    MUTANT("never queued", BOTH, e.queued = false),
    MUTANT("effect certain", BOTH, e.effectUncertain = false),
    MUTANT("admitted at the cutoff", BOTH, e.admittedUs = e.transfer.deadlineUs),
    MUTANT("needs preoperational", BOTH, e.transfer.requirePreOperational = true),
};
void bestEffortEntry() {
    const uint64_t start = ACQUIRED;
    L::ReadTarget current = target();
    L::BestEffortStop c;
    L::BestEffortStop blank;
    const Saved<L::BestEffortStop> empty(blank);
    // Preparation refuses each wrong input and leaves output alone.
    L::ReadTarget invalid = current;
    invalid.nodeGeneration = 0;
    CHECK(refused(L::prepareBestEffortStop(blank, invalid, 41, start, DEADLINE, options())));
    invalid = current;
    invalid.channel.clientToServerId = invalid.channel.serverToClientId;
    CHECK(refused(L::prepareBestEffortStop(blank, invalid, 41, start, DEADLINE, options())));
    CHECK(refused(L::prepareBestEffortStop(blank, current, 0, start, DEADLINE, options())));
    CHECK(refused(L::prepareBestEffortStop(blank, current, 41, start, start, options())));
    L::ReadOptions zero = options();
    zero.queueTimeoutUs = 0;
    CHECK(refused(L::prepareBestEffortStop(blank, current, 41, start, DEADLINE, zero)));
    CHECK(empty.same(blank));

    CHECK(L::prepareBestEffortStop(c, current, 41, start, DEADLINE, options()));
    L::PreparedAction work;
    CHECK(L::nextBestEffortStop(c, current, start, work) && work.ready);
    // Next is keyed to the frozen target.
    const Saved<L::BestEffortStop> active(c);
    for (unsigned which = 0; which < TARGET_MUTANTS; ++which) {
        L::ReadTarget wrong = current;
        mutateTarget(wrong, which);
        L::PreparedAction output;
        output.eligibleUs = 12345;
        const Saved<L::PreparedAction> untouched(output);
        CHECK(refused(L::nextBestEffortStop(c, wrong, start, output)) && untouched.same(output));
        CHECK(active.same(c));
    }
    // Before admission no terminal can settle it.
    C::SdoCompletion valid = failed(work.transfer, start + 2, start + 5,
                                    C::SdoOutcome::UNCONFIRMED_SENT, true);
    valid.transmissionConfirmed = true;
    CHECK(refused(L::advanceBestEffortStop(c, valid, start + 10)));
    CHECK(active.same(c));
    CHECK(L::noteBestEffortAdmission(c, start));

    const auto advance = [](L::BestEffortStop& context, const C::SdoCompletion& e, uint64_t at) {
        return L::advanceBestEffortStop(context, e, at);
    };
    const Times t = {start, DEADLINE, start + 10};
    refuseEach(bestEffortMutants, c, valid, t, advance, "best-effort");
    CHECK(advance(c, valid, t.now));
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::STOP_SENT_UNCONFIRMED &&
          c.execution == M::ActionExecution::UNKNOWN && c.commandEvidencePresent);
    const Saved<L::BestEffortStop> closed(c);
    CHECK(refused(advance(c, valid, t.now + 1)));
    CHECK(closed.same(c));
}

// The prerequisite terminal that prepareDriverSettings and prepareStop consume rather than advance.
// There is no live work to compare, so only what the reservation and evidence rules state is checked.
const Mutant controlMutants[] = {
    ROUTE_MUTANTS,
    MUTANT("no operation", BOTH, e.transfer.operation = 0),
    MUTANT("no request", BOTH, e.transfer.request = 0),
    MUTANT("no step", BOTH, e.transfer.step = 0),
    MUTANT("later node epoch", BOTH, ++e.nodeGeneration),
    MUTANT("older node epoch", BOTH, --e.nodeGeneration),
    MUTANT("no node anchor", BOTH, e.nodeGeneration = 0),
    MUTANT("statusword object", BOTH, e.transfer.index = 0x6041),
    MUTANT("mode object", BOTH, e.transfer.index = 0x6060),
    MUTANT("subindex", BOTH, ++e.transfer.subindex),
    MUTANT("one byte", BOTH, e.transfer.size = 1),
    MUTANT("four bytes", BOTH, e.transfer.size = 4),
    MUTANT("upload", BOTH, e.transfer.direction = C::SdoDirection::UPLOAD),
    MUTANT("cutoff at completion", BOTH, e.transfer.deadlineUs = e.completedUs),
    MUTANT("cutoff earlier", BOTH, e.transfer.deadlineUs = e.completedUs - 1),
    MUTANT("no queue budget", BOTH, e.transfer.queueTimeoutUs = 0),
    MUTANT("no response budget", BOTH, e.transfer.responseTimeoutUs = 0),
    MUTANT("timeout", BOTH, e.outcome = C::SdoOutcome::TIMEOUT),
    MUTANT("server abort", BOTH, e.outcome = C::SdoOutcome::SERVER_ABORT),
    MUTANT("not queued", BOTH, e.queued = false),
    MUTANT("not possible", BOTH, e.transmissionPossible = false),
    MUTANT("no acknowledgement", BOTH, e.protocolAcknowledged = false),
    MUTANT("no effect", BOTH, e.effectPossible = false),
    MUTANT("uncertain", BOTH, e.effectUncertain = true),
    MUTANT("abort valid", BOTH, e.abortValid = true),
    MUTANT("returned bytes", BOTH, e.size = 1),
    MUTANT("admitted before reservation", BOTH, e.admittedUs = ACQUIRED - 1),
    MUTANT("completed before admitted", BOTH, e.admittedUs = e.completedUs + 1),
    MUTANT("completed in the future", BOTH, e.completedUs = t.now + 1),
    MUTANT("different word", BOTH, e.transfer.data[0] = 0x0b),
};
void controlEvidenceEntries() {
    const L::AxisReservation live = reservation();
    uint64_t now = ACQUIRED;
    const C::SdoCompletion control = controlWord(0x000f, now);
    L::ReadContext state;
    CHECK(L::prepareState(state, target(), 8, now, DEADLINE, options(),
                          mask({L::StateBlock::STATUS, L::StateBlock::MONITORS})));
    finishRead(state, now, Answers());
    L::DriverContext prior;
    CHECK(L::prepareDriverRead(prior, target(), 9, now, DEADLINE, options(),
                               L::DriverGroup::MOTION_PARAMETERS));
    finishRead(prior.read, now, Answers());
    L::DriverSettings candidate;
    candidate.positionSpeedRpm = 10;
    candidate.rampUpTimeMs = candidate.rampDownTimeMs = 20;

    L::DriverSettingsContext settings;
    settings.operation = 0xbeef;
    L::ActionContext stop;
    stop.operation = 0xbeef;
    L::ActionContext validated;
    validated.operation = 0xbeef;
    const Times t = {ACQUIRED, DEADLINE, now};
    const auto prepareSettings = [&](L::DriverSettingsContext& out, const C::SdoCompletion& e, uint64_t at) {
        return L::prepareDriverSettings(out, live, live.target, prior, state, e, candidate, 41, at, DEADLINE,
                                        options(), 100000);
    };
    const auto prepareStop = [&](L::ActionContext& out, const C::SdoCompletion& e, uint64_t at) {
        return L::prepareStop(out, live, state, e, L::StopKind::QUICK_STOP, 41, at, DEADLINE,
                              actionOptions());
    };
    const auto validateStop = [&](L::ActionContext& out, const C::SdoCompletion& e, uint64_t at) {
        return L::validateStop(out, live, state, e, L::StopKind::QUICK_STOP, 41, at, DEADLINE,
                               actionOptions());
    };
    refuseEach(controlMutants, settings, control, t, prepareSettings, "settings control");
    refuseEach(controlMutants, stop, control, t, prepareStop, "stop control");
    refuseEach(controlMutants, validated, control, t, validateStop, "validate stop control");
    // Wrong live reservation of the same evidence. A changed reservation owner alone is legal for a
    // prerequisite prepared under it, so only the route and acquisition fields can refuse.
    const Saved<L::DriverSettingsContext> settingsBefore(settings);
    const Saved<L::ActionContext> stopBefore(stop);
    for (unsigned which = 0; which < TARGET_MUTANTS; ++which) {
        L::AxisReservation wrong = live;
        mutateTarget(wrong.target, which);
        CHECK(refused(L::prepareDriverSettings(settings, wrong, wrong.target, prior, state, control, candidate, 41,
                                               now, DEADLINE, options(), 100000)));
        CHECK(refused(L::prepareStop(stop, wrong, state, control, L::StopKind::QUICK_STOP, 41, now,
                                     DEADLINE, actionOptions())));
        CHECK(settingsBefore.same(settings) && stopBefore.same(stop));
    }
    CHECK(prepareSettings(settings, control, now));
    CHECK(prepareStop(stop, control, now));
}

// prepareEscalation consumes the running stop's own acknowledgement.
void escalationEvidence() {
    const L::AxisReservation live = reservation();
    uint64_t now = ACQUIRED;
    const C::SdoCompletion control = controlWord(0x000f, now);
    L::ReadContext state;
    Answers answers;
    answers.monitor = L::MONITOR_RUNNING;
    CHECK(L::prepareState(state, target(), 8, now, DEADLINE, options(),
                          mask({L::StateBlock::STATUS, L::StateBlock::MONITORS})));
    finishRead(state, now, answers);
    L::ActionContext running;
    CHECK(L::prepareStop(running, live, state, control, L::StopKind::HALT, 41, now, DEADLINE,
                         actionOptions()));
    L::PreparedAction work;
    CHECK(L::nextAction(running, live, target(), now, work) && work.ready);
    CHECK(L::noteActionTransmission(running, live, target(), now));
    ++now;
    CHECK(L::advanceAction(running, live, target(), acknowledged(work.transfer, now, now), now));
    for (unsigned i = 0; i < 5; ++i) {
        CHECK(L::nextAction(running, live, target(), now, work) && work.ready);
        ++now;
        const uint32_t raw = actionAnswer(running, work.transfer.index, work.transfer.subindex);
        CHECK(L::advanceAction(running, live, target(), acknowledged(work.transfer, now, now, raw), now));
    }
    CHECK(running.phase == L::ActionPhase::WAITING && running.observationKnown);

    L::ActionContext output;
    output.operation = 0xbeef;
    const Times t = {ACQUIRED, DEADLINE, now};
    const auto escalate = [&](L::ActionContext& out, const C::SdoCompletion& e, uint64_t at) {
        L::ActionContext& copy = running;
        const C::SdoCompletion kept = copy.commandHistory[0];
        copy.commandHistory[0] = e;
        const M::Status status = L::prepareEscalation(out, copy, 42, at, DEADLINE, actionOptions());
        copy.commandHistory[0] = kept;
        return status;
    };
    refuseEach(controlMutants, output, running.commandHistory[0], t, escalate, "escalation");
    C::SdoCompletion otherWord = running.commandHistory[0];
    otherWord.transfer.data[0] = 0x0f; // 000F is an allowed predecessor, but not what a HALT sent.
    otherWord.transfer.data[1] = 0;
    CHECK(refused(escalate(output, otherWord, now)));
    CHECK(L::prepareEscalation(output, running, 42, now, DEADLINE, actionOptions()));
    CHECK(output.kind == L::StopKind::QUICK_STOP && output.predecessorWord == 0x010f &&
          !output.predecessorDocumented);
    // A corrupted later cycle cannot justify it either.
    L::ActionContext second;
    const Saved<L::ActionContext> blank(second);
    L::ActionContext stale = running;
    stale.statusObservation.fields[0].earliestUs = running.commandHistory[0].completedUs - 1;
    CHECK(refused(L::prepareEscalation(second, stale, 42, now, DEADLINE, actionOptions())));
    stale = running;
    stale.statusObservation.modeRaw = 3;
    CHECK(refused(L::prepareEscalation(second, stale, 42, now, DEADLINE, actionOptions())));
    stale = running;
    stale.monitorObservation.monitor = L::MONITOR_ALARM;
    CHECK(refused(L::prepareEscalation(second, stale, 42, now, DEADLINE, actionOptions())));
    stale = running;
    stale.commandEvidenceMask = 0;
    CHECK(refused(L::prepareEscalation(second, stale, 42, now, DEADLINE, actionOptions())));
    CHECK(blank.same(second));
}

// A position parameter candidate consumes no terminal and takes no route, so the entry has nothing
// to correlate. What it owns is atomic refusal, the fixed native ranges and epochs of its target.
void positionParameters() {
    M::AxisConfig axis = axisFor(target());
    axis.supportedRelativeBases = 7;
    M::PositionRequest request;
    request.configurationGeneration = axis.binding.configurationGeneration;
    request.value = M::Rational(10);
    L::DriverSettings settings;
    settings.positionSpeedRpm = 3000;
    settings.rampUpTimeMs = settings.rampDownTimeMs = 2000;
    L::PreparedPositionParameters output;
    output.targetNative = 0x1234;
    const Saved<L::PreparedPositionParameters> blank(output);
    const auto refuse = [&](L::SettingsModel model, const M::PositionRequest& r, const M::AxisConfig& a,
                            const M::AxisReference* reference, const L::DriverSettings& s, uint64_t now) {
        CHECK(refused(L::preparePositionParameters(output, model, r, a, reference, s, now)));
        CHECK(blank.same(output));
    };
    refuse(L::SettingsModel::UNKNOWN, request, axis, nullptr, settings, 100);
    refuse(static_cast<L::SettingsModel>(9), request, axis, nullptr, settings, 100);
    refuse(L::SettingsModel::CL86_C, request, axis, nullptr, settings, uint64_t(INT64_MAX) + 1);
    for (unsigned which = 0; which < 3; ++which) {
        L::DriverSettings wide = settings;
        if (which == 0)
            wide.positionSpeedRpm = 3001;
        if (which == 1)
            wide.rampUpTimeMs = 2001;
        if (which == 2)
            wide.rampDownTimeMs = 2001;
        refuse(L::SettingsModel::OL86_C, request, axis, nullptr, wide, 100);
    }
    M::PositionRequest wrong = request;
    wrong.wrapped = true;
    refuse(L::SettingsModel::CL86_C, wrong, axis, nullptr, settings, 100);
    wrong = request;
    ++wrong.configurationGeneration;
    refuse(L::SettingsModel::CL86_C, wrong, axis, nullptr, settings, 100);
    --wrong.configurationGeneration;
    --wrong.configurationGeneration;
    refuse(L::SettingsModel::CL86_C, wrong, axis, nullptr, settings, 100);
    wrong = request;
    wrong.value = M::Rational(1000001);
    refuse(L::SettingsModel::CL86_C, wrong, axis, nullptr, settings, 100);
    wrong.value = M::Rational(-1000001);
    refuse(L::SettingsModel::CL86_C, wrong, axis, nullptr, settings, 100);
    // A witness with a wrong epoch, or one later than this call's time, is refused.
    M::AxisReference reference;
    reference.binding = axis.binding;
    reference.referenceGeneration = axis.referenceGeneration;
    reference.nativeKnown = true;
    reference.source = M::ScaleSource::QUALIFIED;
    reference.observedUs = 100;
    reference.nowUs = 100;
    M::AxisReference badReference = reference;
    ++badReference.binding.configurationGeneration;
    refuse(L::SettingsModel::CL86_C, request, axis, &badReference, settings, 100);
    badReference = reference;
    ++badReference.referenceGeneration;
    refuse(L::SettingsModel::CL86_C, request, axis, &badReference, settings, 100);
    badReference = reference;
    badReference.nowUs = 101;
    refuse(L::SettingsModel::CL86_C, request, axis, &badReference, settings, 100);
    // Output inside an input.
    L::PreparedPositionParameters holder;
    holder.settings = settings;
    const M::Status overlap = L::preparePositionParameters(holder, L::SettingsModel::CL86_C, request,
                                                           axis, nullptr, holder.settings, 100);
    CHECK(refused(overlap) && overlap.detail == int32_t(L::PositionParameterError::OVERLAP));

    CHECK(L::preparePositionParameters(output, L::SettingsModel::CL86_C, request, axis, &reference,
                                       settings, 100));
    CHECK(output.targetNative == 10 && output.settings.positionSpeedRpm == 3000);
}
} // namespace

int main() {
    readEntries();
    driverEntries();
    getterTargets();
    gettersRefuseOtherKinds();
    settingsEntry(SettingsKind::MOTION, "driver settings");
    settingsEntry(SettingsKind::IO, "I/O settings");
    settingsEntry(SettingsKind::CONTROL, "control settings");
    actionEntry(M::ActionKind::STOP, L::StopKind::HALT, "halt");
    actionEntry(M::ActionKind::STOP, L::StopKind::QUICK_STOP, "quick stop");
    actionEntry(M::ActionKind::CLEAR_ALARM, L::StopKind::HALT, "clear alarm");
    actionEntry(M::ActionKind::SET_DEVICE_POSITION, L::StopKind::HALT, "counter clear");
    bestEffortEntry();
    controlEvidenceEntries();
    escalationEvidence();
    positionParameters();
    std::printf("evidence_corruption: %u checks passed\n", checks);
    return 0;
}
