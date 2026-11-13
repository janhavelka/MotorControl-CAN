// SPDX-License-Identifier: MIT
// Real service/owner rig for the alarm-clear and device-zero actions, with synthetic controller
// traffic only. No physical drive, firmware qualification or implicit enabling is represented.
// Define `checks` and CHECK first, as for OwnerRig.h. Test executables use one TU, so the
// anonymous namespace below is private to the including suite.
#pragma once
#ifndef CHECK
#error "Define the suite's checks counter and CHECK macro before including ReferenceRig.h"
#endif
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;
#include "OwnerRig.h"
namespace {
using OwnerRig::budget;
using OwnerRig::frame;
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
inline L::ReadTarget target(unsigned node = 1) {
    L::ReadTarget value;
    value.channel.binding.logicalAxisId = 100 + node;
    value.channel.binding.endpoint.profile = 41;
    value.channel.binding.endpoint.address = node;
    value.channel.binding.bindingGeneration = 3;
    value.channel.binding.configurationGeneration = 7;
    value.channel.clientToServerId = 0x600 + node;
    value.channel.serverToClientId = 0x580 + node;
    value.nodeGeneration = 11;
    return value;
}
inline L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = value.responseTimeoutUs = 100;
    return value;
}
inline M::Frame reply(const C::SdoTransfer& request, uint32_t raw = 0, bool abort = false) {
    M::Frame value = frame(request.channel.serverToClientId, 8);
    value.data[0] = abort                                            ? 0x80
                    : request.direction == C::SdoDirection::DOWNLOAD ? 0x60
                    : request.size == 1                              ? 0x4f
                    : request.size == 2                              ? 0x4b
                                                                     : 0x43;
    value.data[1] = uint8_t(request.index);
    value.data[2] = uint8_t(request.index >> 8);
    value.data[3] = request.subindex;
    if (abort || request.direction == C::SdoDirection::UPLOAD)
        for (unsigned i = 0; i < 4; ++i)
            value.data[4 + i] = uint8_t(raw >> (8 * i));
    return value;
}
inline M::Frame emcyFrame(unsigned node, uint16_t code, uint8_t marker = 0xa5) {
    M::Frame value = frame(0x80 + node, 8);
    value.data[0] = uint8_t(code);
    value.data[1] = uint8_t(code >> 8);
    value.data[2] = 1;
    value.data[7] = marker;
    return value;
}
inline uint16_t frameIndex(const M::Frame& f) {
    return uint16_t(f.data[1] | (f.data[2] << 8));
}
inline bool isDownload(const M::Frame& f) {
    return (f.data[0] & 0xe0) == 0x20;
}
struct Wire : OwnerRig::WireBase {
    L::ActionContext* action = nullptr;
    const L::AxisReservation* reservation = nullptr;
    L::ReadTarget* current = nullptr;
    M::AxisConfig* axis = nullptr;
    M::AxisReference* reference = nullptr;
    unsigned notices = 0; ///< Guard calls for the action's own downloads.
    static bool guard(void* context, O::SequenceId, const C::SdoTransfer& work, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        if (!w.action || work.operation != w.action->operation)
            return true;
        // The alarm clear's controlword method writes 6040 0x0080 and 0x0000; nothing else writes 6040.
        CHECK(w.reservation && w.current &&
              (work.index == 0x2301 || work.index == 0x2302 ||
               (work.index == 0x6040 && w.action->clearMethod == L::AlarmClearMethod::CONTROLWORD_BIT7)));
        ++w.notices;
        if (work.index == 0x2302) {
            CHECK(w.axis && w.reference);
            return bool(L::noteActionTransmission(*w.action, *w.reservation, *w.current, *w.axis,
                                                  *w.reference, now));
        }
        return bool(L::noteActionTransmission(*w.action, *w.reservation, *w.current, now));
    }
    O::Transport callbacks() {
        O::Transport value = plumbing();
        value.beforeTransmit = guard;
        return value;
    }
};
// Where a download fails. Each later stage is one more boundary the write crossed.
enum class Stage : uint8_t {
    BEFORE_ADMISSION, ///< Yielded but never admitted; the application ends the action locally.
    QUEUED,           ///< Admitted, cancelled in the owner queue: never handed to the SDK.
    SDK_ACCEPTED,     ///< Handed to the controller, which never reports TX or retires it.
    TX_REPORTED,      ///< Controller reported TX; no reply within the response budget.
    ABORTED,          ///< The server answered with an SDO abort.
    /** The positive ACK settled before the cutoff, and the application processes it at the cutoff:
     * the ACK is kept, and the action ends DEADLINE. */
    ACK_THEN_DEADLINE,
    /** A positive reply arrives exactly at the overall cutoff. The cutoff wins: the owner ends
     * the request TIMEOUT with its frame out and no accepted reply. */
    REPLY_AT_DEADLINE
};
const Stage STAGES[] = {Stage::BEFORE_ADMISSION, Stage::QUEUED,  Stage::SDK_ACCEPTED,
                        Stage::TX_REPORTED,      Stage::ABORTED, Stage::ACK_THEN_DEADLINE,
                        Stage::REPLY_AT_DEADLINE};
// The stages where the request frame may have left.
inline bool sent(Stage stage) {
    return stage >= Stage::SDK_ACCEPTED;
}
// The stages that leave the SDO route unresolved: a frame may be out with no accepted reply.
inline bool ambiguous(Stage stage) {
    return stage == Stage::SDK_ACCEPTED || stage == Stage::TX_REPORTED ||
           stage == Stage::REPLY_AT_DEADLINE;
}
// The stages whose write ends with its positive acknowledgement kept.
inline bool acknowledged(Stage stage) {
    return stage == Stage::ACK_THEN_DEADLINE;
}
// The action's outcome when its download fails at this stage.
inline M::ActionOutcome outcomeAt(Stage stage) {
    return stage <= Stage::QUEUED     ? M::ActionOutcome::CANCELLED
           : stage == Stage::ABORTED  ? M::ActionOutcome::PROTOCOL_REJECTED
           : stage >= Stage::ACK_THEN_DEADLINE ? M::ActionOutcome::DEADLINE
                                       : M::ActionOutcome::SERVICE_TIMEOUT;
}
// The SDO uploads these actions and their prerequisite reads may send. Anything else, and any
// download other than the action's own object, would be enable, motion, save, restore or retry.
inline bool permittedUpload(uint16_t index) {
    const uint16_t allowed[] = {0x1001, 0x1003, 0x2100, 0x2101, 0x2102,
                                0x603f, 0x6041, 0x6061, 0x6064, 0x606c};
    for (uint16_t value : allowed)
        if (value == index)
            return true;
    return false;
}
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    O::SequenceId sequence;
    L::AxisReservation reservation;
    L::ReadTarget current = target();
    L::ReadContext state;
    M::AxisConfig axis;
    M::AxisReference reference;
    M::PreparedTarget prepared;
    unsigned invalidations = 0;
    uint64_t now = 0, deadline = 10000;
    uint32_t operation = 0;
    int32_t feedbackPosition = 123; ///< 6064 the fake drive reports to prerequisite reads.
    int32_t feedbackVelocity = 0;   ///< 606C the fake drive reports to every read.
    uint8_t errorRegister = 1;      ///< 1001 reported with a nonzero 603F.
    /** 6041 the fake drive reports to every read. All bits set by default: no low-bit pattern or dialect
     * bit decides a mode 1 clear or a counter zero. A mode 3 clear completes only with low bits 000 and
     * bits 3 and 7 clear, so its tests set what node 1 read (C018 alarmed, 8010 after the clear). */
    uint16_t statusword = 0xffff;
    /** prepare's alarm clear method; commands() writes that method's two values. */
    L::AlarmClearMethod clearMethod = L::AlarmClearMethod::OBJECT_2301;
    // A second node on the same interface, for fault isolation.
    O::SequenceId sequence2;
    L::AxisReservation reservation2;
    L::ReadTarget current2 = target(2);
    unsigned invalidations2 = 0;

    static void invalidated(void* context, const C::NetworkInvalidationEvent& event) {
        Rig& r = *static_cast<Rig*>(context);
        ++r.invalidations;
        if (event.cause == C::NetworkInvalidation::CONFIGURATION) {
            // This callback synchronizes a withdrawal already made at possible
            // trigger TX. A second invalidation would create the wrong epoch.
            CHECK(r.axis.binding.configurationGeneration ==
                  event.previous.binding.configurationGeneration + 1);
            CHECK(!r.axis.originKnown && !r.reference.nativeKnown);
        } else {
            CHECK(M::invalidateAxisReference(r.axis, r.reference));
            r.current.channel.binding = r.axis.binding;
            r.current.nodeGeneration = event.generation;
        }
    }
    static void invalidated2(void* context, const C::NetworkInvalidationEvent&) {
        ++static_cast<Rig*>(context)->invalidations2;
    }
    explicit Rig(M::Generation configuration = 7, M::Generation confidence = 1)
        : owner(services, wire.callbacks(), 73) {
        current.channel.binding.configurationGeneration = configuration;
        axis.binding = current.channel.binding;
        axis.referenceGeneration = confidence;
        axis.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::READBACK);
        axis.units.encoder.countsPerUnit = M::UnitScale(4000, 1, M::ScaleSource::READBACK);
        axis.units.encoder.sourceId = 6;
        axis.supportedRelativeBases = 1;
        axis.originKnown = axis.encoderOriginKnown = axis.softLimitsKnown = true;
        axis.originNative = 10;
        axis.encoderOriginNative = 20;
        axis.originSource = axis.encoderOriginSource = M::ScaleSource::READBACK;
        axis.softMinimum = -10000;
        axis.softMaximum = 10000;
        reference.binding = axis.binding;
        reference.referenceGeneration = axis.referenceGeneration;
        reference.nativeKnown = reference.idle = reference.stationary = true;
        reference.nativePosition = 123;
        reference.source = M::ScaleSource::QUALIFIED;
        reference.maximumAgeUs = 1000;
        M::PositionRequest request;
        request.configurationGeneration = axis.binding.configurationGeneration;
        request.value = M::Rational(10);
        CHECK(M::preparePosition(request, axis, nullptr, prepared));
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, now));
        CHECK(services.configure(0, current.channel, true, true));
        C::NetworkRoute network;
        network.binding = current.channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        C::NetworkObserver observer;
        observer.context = this;
        observer.invalidate = invalidated;
        CHECK(services.configureNetwork(0, network, observer, 0));
        CHECK(owner.beginSequence(current.channel.binding, 1, now, sequence));
        reservation.target = current;
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = owner.identity();
        reservation.sequence = uint32_t(sequence.slot + 1);
        reservation.generation = sequence.generation;
        reservation.acquiredUs = owner.sequence(sequence)->acquiredUs;
        wire.reservation = &reservation;
        wire.current = &current;
        wire.axis = &axis;
        wire.reference = &reference;
    }
    // Node 2 on slot 1: its own route, network epoch and reservation, nothing shared with node 1.
    void addSecondNode() {
        CHECK(services.configure(1, current2.channel, true, true));
        C::NetworkRoute network;
        network.binding = current2.channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x702;
        network.emcyId = 0x82;
        C::NetworkObserver observer;
        observer.context = this;
        observer.invalidate = invalidated2;
        CHECK(services.configureNetwork(1, network, observer, now));
        CHECK(owner.beginSequence(current2.channel.binding, 2, now, sequence2));
        reservation2.target = current2;
        reservation2.model = L::SettingsModel::OL86_C;
        reservation2.owner = owner.identity();
        reservation2.sequence = uint32_t(sequence2.slot + 1);
        reservation2.generation = sequence2.generation;
        reservation2.acquiredUs = owner.sequence(sequence2)->acquiredUs;
    }
    void begin() { CHECK(owner.beginOperation(sequence, ++operation, deadline, now)); }
    void turn(uint64_t time, const O::ServiceBudget& work = budget()) {
        now = time;
        CHECK(owner.service(now, work));
    }
    O::RequestId admit(const C::SdoTransfer& transfer, const O::SequenceId* on = nullptr) {
        O::BusRequest work;
        work.sequence = on ? *on : sequence;
        work.transfer = transfer;
        O::RequestId id;
        CHECK(owner.admit(work, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    C::SdoCompletion result(O::RequestId id, bool release = true) {
        const O::Completion* terminal = owner.result(id);
        CHECK(terminal);
        const C::SdoCompletion value = terminal->service;
        if (release)
            CHECK(owner.release(id));
        return value;
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work, uint32_t raw = 0, bool abort = false,
                              const O::SequenceId* on = nullptr) {
        const O::RequestId id = admit(work, on);
        turn(now);
        wire.push(reply(work, raw, abort), now + 1);
        turn(now + 1, budget(8, 0, 8, 0));
        return result(id);
    }
    uint32_t raw(const C::SdoTransfer& work, uint16_t monitor, uint8_t mode, uint16_t error,
                 int32_t position) const {
        switch (work.index) {
        case 0x1001:
            return error ? errorRegister : 0;
        case 0x1003:
            return work.subindex ? 0x12340000u + work.subindex : 2;
        case 0x2100:
            return monitor;
        case 0x2101:
            return 0xa580;
        case 0x2102:
            return 0xf008;
        case 0x603f:
            return error;
        case 0x6041:
            return statusword;
        case 0x6061:
            return mode;
        case 0x6064:
            return uint32_t(position);
        case 0x606c:
            return uint32_t(feedbackVelocity);
        default:
            CHECK(false);
            return 0;
        }
    }
    void acquire(uint16_t monitor, uint8_t mode = 1, uint16_t error = 0xff05, uint8_t mask = 0x1f) {
        begin();
        CHECK(L::prepareState(state, current, operation, now, deadline, options(), mask));
        while (state.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, now, work) && work.ready);
            const C::SdoCompletion terminal =
                exchange(work.transfer, raw(work.transfer, monitor, mode, error, feedbackPosition));
            CHECK(L::advanceRead(state, terminal, now));
        }
        CHECK(state.state == M::ReadState::SUCCEEDED);
    }
    L::ActionOptions policy() const {
        L::ActionOptions value;
        value.service = options();
        value.observationIntervalUs = 10;
        value.maximumAgeUs = 1400; // A-20: at least 7 x (queue + response), for the counter clear.
        return value;
    }
    void prepare(L::ActionContext& action, bool position) {
        begin();
        if (position)
            CHECK(L::prepareSetDevicePosition(action, reservation, state, 0, axis, operation, now,
                                              deadline, policy()));
        else
            CHECK(L::prepareClearAlarm(action, reservation, state, operation, now, deadline,
                                       policy(), clearMethod));
        wire.action = &action;
    }
    L::PreparedAction next(const L::ActionContext& action) {
        L::PreparedAction work;
        CHECK(L::nextAction(action, reservation, current, now, work));
        return work;
    }
    void field(L::ActionContext& action, uint32_t rawValue = 0, bool abort = false) {
        const L::PreparedAction work = next(action);
        CHECK(work.ready);
        const C::SdoCompletion terminal = exchange(work.transfer, rawValue, abort);
        CHECK(L::advanceAction(action, reservation, current, terminal, now));
    }
    void command(L::ActionContext& action, uint16_t index, uint16_t value) {
        const auto work = next(action);
        CHECK(work.ready && work.transfer.index == index && work.transfer.subindex == 0 &&
              work.transfer.direction == C::SdoDirection::DOWNLOAD && work.transfer.size == 2 &&
              work.transfer.channel.binding.configurationGeneration == 7);
        CHECK(work.transfer.data[0] == uint8_t(value) &&
              work.transfer.data[1] == uint8_t(value >> 8));
        const unsigned sent = wire.sentCount;
        field(action);
        CHECK(wire.sentCount == sent + 1);
        const uint8_t expected[] = {
            0x2b, uint8_t(index), uint8_t(index >> 8), 0, uint8_t(value), uint8_t(value >> 8), 0,
            0};
        CHECK(wire.sent[sent].id == 0x601 && wire.sent[sent].payloadLength == 8 &&
              !std::memcmp(wire.sent[sent].data, expected, sizeof expected));
    }
    void synchronize() {
        CHECK(axis.binding.configurationGeneration == 8 && axis.referenceGeneration == 2);
        CHECK(owner.advanceConfiguration(sequence, current.channel.binding, now));
        CHECK(invalidations == 1 && axis.binding.configurationGeneration == 8 &&
              axis.referenceGeneration == 2);
    }
    void cycle(L::ActionContext& action, uint16_t monitor = 0, int32_t position = 0,
               uint16_t error = 0xff05, uint8_t mode = 1) {
        const uint32_t before = action.observations;
        unsigned fields = 0;
        do {
            const auto work = next(action);
            CHECK(work.ready && work.transfer.direction == C::SdoDirection::UPLOAD);
            CHECK(work.transfer.channel.binding.configurationGeneration ==
                  current.channel.binding.configurationGeneration);
            field(action, raw(work.transfer, monitor, mode, error, position));
            CHECK(++fields <= 7);
        } while (action.state == M::ActionState::ACTIVE &&
                 (action.observations == before || action.finalErrorRead));
    }
    void commands(L::ActionContext& action, bool position) {
        if (position) {
            command(action, 0x2302, 0);
            command(action, 0x2302, 1);
            command(action, 0x2302, 0);
            synchronize();
        } else if (clearMethod == L::AlarmClearMethod::CONTROLWORD_BIT7) {
            command(action, 0x6040, 0x0080);
            command(action, 0x6040, 0x0000);
        } else {
            command(action, 0x2301, 1);
            command(action, 0x2301, 0);
        }
    }
    // Frames sent from `from` on: only permitted uploads and exactly `writes` downloads of `index`,
    // all to node 1. No enable, controlword, mode, target, save, restore, NMT or retry.
    void onlyActionTraffic(unsigned from, uint16_t index, unsigned writes) const {
        unsigned downloads = 0;
        for (unsigned i = from; i < wire.sentCount; ++i) {
            const M::Frame& f = wire.sent[i];
            CHECK(f.id == 0x601 && f.payloadLength == 8);
            if (isDownload(f)) {
                CHECK(frameIndex(f) == index && f.data[3] == 0 && f.data[0] == 0x2b);
                ++downloads;
            } else {
                CHECK(f.data[0] == 0x40 && permittedUpload(frameIndex(f)));
            }
        }
        CHECK(downloads == writes);
    }
    unsigned uploadsOf(unsigned from, uint16_t index) const {
        unsigned count = 0;
        for (unsigned i = from; i < wire.sentCount; ++i)
            if (!isDownload(wire.sent[i]) && frameIndex(wire.sent[i]) == index)
                ++count;
        return count;
    }
    void checkWithdrawn() {
        CHECK(axis.binding.configurationGeneration == 8 && axis.referenceGeneration == 2 &&
              !axis.originKnown && !axis.encoderOriginKnown && !axis.softLimitsKnown &&
              !reference.nativeKnown);
        // Validity was withdrawn; the configured values stay.
        CHECK(axis.softMinimum == -10000 && axis.softMaximum == 10000 &&
              axis.units.commandStepsPerMotorTurn.numerator == 3200);
        CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, now));
        L::StateObservation stale;
        CHECK(!L::getStateBlock(state, L::StateBlock::FEEDBACK, current, now, 1000, stale));
        M::PositionRequest request;
        request.configurationGeneration = axis.binding.configurationGeneration;
        request.value = M::Rational(1);
        request.frame = M::CoordinateFrame::MOTOR;
        M::PreparedTarget output = prepared;
        const Saved<M::PreparedTarget> saved(output);
        CHECK(!M::preparePosition(request, axis, nullptr, output));
        saved.unchanged(output); // No new physical-coordinate target without an origin.
        request.frame = M::CoordinateFrame::NATIVE;
        request.relative = true;
        CHECK(!M::preparePosition(request, axis, &reference, output));
        saved.unchanged(output); // A zero report never restores a relative witness.
    }
    // What failCommand leaves for later checks. `admitted` is false for BEFORE_ADMISSION.
    struct Failure {
        bool admitted = false;
        O::RequestId id;
        C::SdoTransfer work;
        C::SdoCompletion terminal;
        unsigned sent = 0; ///< wire.sentCount before the failing download.
    };
    // Drive the action's current download to `stage` and end the action there, through the real
    // owner. The terminal stays retained in the owner until the caller releases failure.id.
    Failure failCommand(L::ActionContext& action, Stage stage) {
        Failure f;
        if (stage >= Stage::ACK_THEN_DEADLINE)
            turn(deadline - 2, O::ServiceBudget());
        const L::PreparedAction work = next(action);
        CHECK(work.ready && work.transfer.direction == C::SdoDirection::DOWNLOAD);
        f.work = work.transfer;
        f.sent = wire.sentCount;
        if (stage == Stage::BEFORE_ADMISSION) {
            CHECK(L::advanceAction(action, reservation, current, M::ActionOutcome::CANCELLED, now));
            return f;
        }
        f.admitted = true;
        f.id = admit(work.transfer);
        switch (stage) {
        case Stage::QUEUED:
            CHECK(owner.cancel(f.id, now) == O::Cancel::CANCELLED);
            break;
        case Stage::SDK_ACCEPTED:
            wire.autoReport = wire.retirement = false;
            turn(now);
            turn(now + 101, budget(0, 0, 8, 0));
            wire.autoReport = wire.retirement = true;
            break;
        case Stage::TX_REPORTED:
            turn(now);
            turn(now + 1, budget(8, 0, 8, 0)); // The owner takes the controller's TX report.
            turn(now + 100, budget(8, 0, 8, 0));
            CHECK(owner.result(f.id) && owner.result(f.id)->service.transmissionConfirmed);
            break;
        case Stage::ABORTED:
            turn(now);
            wire.push(reply(work.transfer, 0x06090030, true), now + 1);
            turn(now + 1, budget(8, 0, 8, 0));
            break;
        case Stage::ACK_THEN_DEADLINE:
            turn(now);
            wire.push(reply(work.transfer), now + 1);
            turn(now + 1, budget(8, 0, 8, 0));
            CHECK(owner.result(f.id) && owner.result(f.id)->service.outcome == C::SdoOutcome::SUCCESS);
            turn(deadline, O::ServiceBudget());
            break;
        default:
            turn(now);
            wire.push(reply(work.transfer), deadline);
            turn(deadline, budget(8, 0, 8, 0));
            CHECK(owner.result(f.id) && owner.result(f.id)->service.outcome == C::SdoOutcome::TIMEOUT);
            break;
        }
        f.terminal = result(f.id, false);
        CHECK(L::advanceAction(action, reservation, current, f.terminal, now));
        return f;
    }
    // A late positive reply to the failed download changes neither the action nor the wire.
    void lateReplyChangesNothing(L::ActionContext& action, const Failure& f) {
        const Saved<L::ActionContext> failed(action);
        const unsigned sentBefore = wire.sentCount;
        wire.push(reply(f.work), now + 1);
        turn(now + 1, budget(8, 0, 8, 0));
        turn(now + 1000, budget());
        failed.unchanged(action);
        CHECK(wire.sentCount == sentBefore);
        L::PreparedAction work;
        CHECK(L::nextAction(action, reservation, current, now, work) && !work.ready);
        CHECK(!L::advanceAction(action, reservation, current, M::ActionOutcome::CANCELLED, now));
        failed.unchanged(action);
    }
    // The opposite of checkWithdrawn: no epoch or validity moved, and the old prepared target still
    // validates. Callers check old reads themselves, with an age that suits them.
    void checkIntact() {
        CHECK(axis.binding.configurationGeneration == 7 && axis.referenceGeneration == 1 &&
              axis.originKnown && axis.encoderOriginKnown && axis.softLimitsKnown &&
              reference.nativeKnown && M::sameBinding(current.channel.binding, axis.binding));
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, now));
    }
};
} // namespace
