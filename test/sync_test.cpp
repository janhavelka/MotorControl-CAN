/** @file sync_test.cpp
 * @brief Synthetic SYNC codec, finite schedule and adversarial local-evidence vectors.
 * EDS 1005 declares default 0x80; these tests qualify no synchronous drive behavior.
 * SPDX-License-Identifier: MIT
 */
#include <MotorControlCAN/canopen/Sync.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;

namespace {
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "sync_test.cpp:%d: %s\n", line, expression);
        std::abort();
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

C::SyncOptions options() {
    C::SyncOptions value;
    value.generation = 7;
    value.originUs = 100;
    value.periodUs = 10;
    value.count = 3;
    return value;
}

C::SyncCompletion completion(const C::SyncWork& work,
                             C::SyncOutcome outcome = C::SyncOutcome::LOCAL_TX) {
    C::SyncCompletion result;
    result.generation = work.generation;
    result.slot = work.slot;
    result.outcome = outcome;
    result.transmissionPossible = outcome == C::SyncOutcome::LOCAL_TX;
    result.transmissionConfirmed = outcome == C::SyncOutcome::LOCAL_TX;
    return result;
}

void codec() {
    M::Frame frame;
    frame.id = 0x321;
    frame.dlc = frame.payloadLength = 8;
    std::memset(frame.data, 0x55, sizeof frame.data);
    const uint32_t rejectedIds[] = {0, 1, 0x7f, 0x101, 0x180, 0x581, 0x5ff, 0x601, 0x67f,
                                   0x6e0, 0x6ff, 0x701, 0x7ff, 0x800, 0x20000080,
                                   0x40000080, 0x80000080, UINT32_MAX};
    for (uint32_t id : rejectedIds) {
        const Saved<M::Frame> before(frame);
        CHECK(C::makeSyncRequest(id, frame).code == M::Err::INVALID_CONFIG);
        CHECK(before.unchanged(frame));
        CHECK(C::checkSyncFrame(frame, id).code == M::Err::INVALID_CONFIG);
    }
    for (uint32_t id : {uint32_t(0x80), uint32_t(0x100), uint32_t(0x181), uint32_t(0x580),
                       uint32_t(0x600), uint32_t(0x680), uint32_t(0x6df), uint32_t(0x700)}) {
        CHECK(C::makeSyncRequest(id, frame));
        CHECK(frame.id == id && frame.dlc == 0 && frame.payloadLength == 0);
        CHECK(!frame.extended && !frame.rtr && !frame.fd && !frame.brs && !frame.esi);
        for (uint8_t byte : frame.data)
            CHECK(byte == 0);
        CHECK(C::checkSyncFrame(frame, id));
    }
    CHECK(C::makeSyncRequest(0x80, frame));
    M::Frame bad = frame;
    bad.id = 0x81;
    CHECK(C::checkSyncFrame(bad, 0x80).code == M::Err::FRAME_ERROR);
    bad = frame;
    bad.dlc = bad.payloadLength = 1;
    CHECK(C::checkSyncFrame(bad, 0x80).code == M::Err::FRAME_ERROR);
    bad = frame;
    bad.extended = true;
    CHECK(C::checkSyncFrame(bad, 0x80).code == M::Err::UNSUPPORTED);
    bad = frame;
    bad.rtr = true;
    CHECK(C::checkSyncFrame(bad, 0x80).code == M::Err::UNSUPPORTED);
    bad = frame;
    bad.fd = true;
    bad.dlc = 15;
    CHECK(C::checkSyncFrame(bad, 0x80).code == M::Err::UNSUPPORTED);
    bad = frame;
    bad.brs = true;
    CHECK(C::checkSyncFrame(bad, 0x80).code == M::Err::FRAME_ERROR);
}

void admission() {
    C::SyncContext context;
    for (unsigned i = 0; i < 7; ++i) {
        C::SyncOptions value = options();
        M::Err expected = M::Err::INVALID_CONFIG;
        switch (i) {
        case 0: value.generation = 0; break;
        case 1: value.periodUs = 0; break;
        case 2: value.count = 0; break;
        case 3: value.configured1005 = 0x40000080; break;
        case 4: value.originUs = 98; break;
        case 5:
            value.originUs = UINT64_MAX - 1;
            expected = M::Err::ILLEGAL_VALUE;
            break;
        case 6:
            value.periodUs = UINT64_MAX;
            expected = M::Err::ILLEGAL_VALUE;
            break;
        }
        const Saved<C::SyncContext> before(context);
        CHECK(C::prepareSync(value, 99, context).code == expected);
        CHECK(before.unchanged(context));
    }
    CHECK(C::prepareSync(options(), 99, context));
    const Saved<C::SyncContext> activeBefore(context);
    C::SyncOptions fresh = options();
    ++fresh.generation;
    CHECK(!C::prepareSync(fresh, 99, context));
    CHECK(activeBefore.unchanged(context));
    CHECK(C::prepareSync(context.options, 99, context).code == M::Err::ILLEGAL_VALUE);
    CHECK(activeBefore.unchanged(context));
    CHECK(C::cancelSync(context, 99));
    CHECK(!C::prepareSync(options(), 99, context));
    CHECK(C::prepareSync(fresh, 99, context));
}

void fixedOriginAndCount() {
    C::SyncContext context;
    CHECK(C::prepareSync(options(), 0, context));
    C::SyncWork work;
    CHECK(C::nextSync(context, 99, work));
    CHECK(!work.available);
    for (uint32_t i = 0; i < 3; ++i) {
        // Delayed first TX must not move the second slot to 118.
        const uint64_t nowUs = 100 + i * 10 + (i == 0 ? 8 : 0);
        CHECK(C::nextSync(context, nowUs, work));
        CHECK(work.available && work.slot == i && work.generation == 7);
        CHECK(work.dueUs == 100 + i * 10 && work.expiresUs == 110 + i * 10);
        C::SyncWork repeated;
        CHECK(C::nextSync(context, nowUs, repeated));
        CHECK(!repeated.available && context.awaitingCompletion);
        CHECK(C::advanceSync(context, completion(work), nowUs));
        CHECK(context.transmittedCount == i + 1 && !context.awaitingCompletion);
    }
    CHECK(context.state == C::SyncState::SUCCEEDED);
    CHECK(context.outcome == C::SyncOutcome::LOCAL_TX && context.transmissionPossible);
    CHECK(C::nextSync(context, 1000, work));
    CHECK(!work.available && context.state == C::SyncState::SUCCEEDED);
    const Saved<C::SyncContext> before(context);
    CHECK(!C::advanceSync(context, context.lastCompletion, 1000));
    CHECK(before.unchanged(context));
}

void deadlinesAndRetirement() {
    for (bool yielded : {false, true}) {
        C::SyncContext context;
        CHECK(C::prepareSync(options(), 0, context));
        C::SyncWork work;
        if (yielded)
            CHECK(C::nextSync(context, 100, work) && work.available);
        C::SyncWork next;
        CHECK(C::nextSync(context, 110, next));
        CHECK(!next.available && context.state == C::SyncState::FAILED);
        CHECK(context.outcome == C::SyncOutcome::MISSED_DEADLINE);
        CHECK(context.awaitingCompletion == yielded);
        C::SyncOptions fresh = options();
        ++fresh.generation;
        fresh.originUs = 200;
        if (yielded) {
            CHECK(!C::prepareSync(fresh, 110, context));
            CHECK(C::advanceSync(context, completion(work), 111));
            CHECK(context.outcome == C::SyncOutcome::MISSED_DEADLINE);
            CHECK(context.transmittedCount == 1 && context.transmissionPossible);
        }
        CHECK(C::prepareSync(fresh, 111, context));
    }
    C::SyncContext context;
    CHECK(C::prepareSync(options(), 0, context));
    C::SyncWork work;
    CHECK(C::nextSync(context, 109, work) && work.available);
    CHECK(C::advanceSync(context, completion(work), 110));
    CHECK(context.state == C::SyncState::FAILED);
    CHECK(context.outcome == C::SyncOutcome::MISSED_DEADLINE);
    CHECK(context.lastCompletion.transmissionConfirmed && context.transmittedCount == 1);
}

void evidence() {
    C::SyncContext context;
    CHECK(C::prepareSync(options(), 0, context));
    C::SyncWork work;
    CHECK(C::nextSync(context, 100, work));
    for (unsigned i = 0; i < 7; ++i) {
        C::SyncCompletion bad = completion(work);
        switch (i) {
        case 0: ++bad.generation; break;
        case 1: ++bad.slot; break;
        case 2: bad.transmissionConfirmed = false; break; // Queue acceptance is not local TX.
        case 3: bad.transmissionPossible = false; break;
        case 4: bad.outcome = C::SyncOutcome::REFUSED; break;
        case 5: bad.outcome = C::SyncOutcome::NONE; break;
        case 6: bad.outcome = static_cast<C::SyncOutcome>(255); break;
        }
        const Saved<C::SyncContext> before(context);
        CHECK(!C::advanceSync(context, bad, 100));
        CHECK(before.unchanged(context));
    }
    const Saved<C::SyncContext> before(context);
    CHECK(!C::advanceSync(context, completion(work), 99));
    CHECK(before.unchanged(context));
    C::SyncWork output;
    output.available = true;
    const Saved<C::SyncWork> outputBefore(output);
    CHECK(!C::nextSync(context, 99, output));
    CHECK(before.unchanged(context) && outputBefore.unchanged(output));
    CHECK(C::nextSync(context, 100, context.pending).code == M::Err::ILLEGAL_VALUE);
    CHECK(before.unchanged(context));
    CHECK(C::cancelSync(context, 101));
    CHECK(context.awaitingCompletion && context.outcome == C::SyncOutcome::CANCELLED);
    CHECK(C::advanceSync(context, completion(work), 102));
    CHECK(context.outcome == C::SyncOutcome::CANCELLED && context.transmissionPossible);
    for (C::SyncOutcome failure : {C::SyncOutcome::REFUSED, C::SyncOutcome::BUS_OFF,
                                  C::SyncOutcome::CANCELLED, C::SyncOutcome::PRODUCER_CONFLICT,
                                  C::SyncOutcome::INVALIDATED, C::SyncOutcome::TRANSPORT_ERROR,
                                  C::SyncOutcome::MISSED_DEADLINE}) {
        C::SyncContext failed;
        CHECK(C::prepareSync(options(), 0, failed));
        CHECK(C::nextSync(failed, 100, work));
        C::SyncCompletion report = completion(work, failure);
        report.transmissionPossible = failure != C::SyncOutcome::REFUSED;
        report.controllerDetail = -123;
        CHECK(C::advanceSync(failed, report, 101));
        CHECK(failed.state == C::SyncState::FAILED && failed.outcome == failure);
        CHECK(failed.transmittedCount == 0 && failed.lastCompletion.controllerDetail == -123);
        CHECK(failed.transmissionPossible == report.transmissionPossible);
        CHECK(C::nextSync(failed, 110, work) && !work.available);
    }
}

void limits() {
    C::SyncContext context;
    C::SyncOptions value = options();
    value.originUs = 0;
    value.periodUs = 10000;
    value.count = 60000; // Exactly ten minutes at the requested 10 ms period.
    value.generation = UINT32_MAX;
    CHECK(C::prepareSync(value, 0, context));
    C::SyncWork work;
    for (uint32_t i = 0; i < value.count; ++i) {
        const uint64_t nowUs = uint64_t(i) * value.periodUs;
        CHECK(C::nextSync(context, nowUs, work) && work.available);
        CHECK(work.dueUs == nowUs && work.slot == i);
        CHECK(C::advanceSync(context, completion(work), nowUs));
    }
    CHECK(context.state == C::SyncState::SUCCEEDED && context.transmittedCount == 60000);
    value.originUs = context.lastNowUs;
    CHECK(!C::prepareSync(value, context.lastNowUs, context)); // Epoch cannot wrap or reuse.
    C::SyncContext nearEnd;
    value.originUs = UINT64_MAX - 1;
    value.periodUs = 1;
    value.count = 1;
    CHECK(C::prepareSync(value, value.originUs, nearEnd));
    CHECK(C::nextSync(nearEnd, value.originUs, work) && work.available);
    CHECK(work.expiresUs == UINT64_MAX);
    CHECK(C::advanceSync(nearEnd, completion(work), UINT64_MAX));
    CHECK(nearEnd.outcome == C::SyncOutcome::MISSED_DEADLINE);
}
void interruptions() {
    C::SyncContext empty;
    const Saved<C::SyncContext> emptyBefore(empty);
    CHECK(!C::advanceSync(empty, C::SyncOutcome::BUS_OFF, 0));
    CHECK(emptyBefore.unchanged(empty));
    for (C::SyncOutcome reason : {C::SyncOutcome::BUS_OFF, C::SyncOutcome::PRODUCER_CONFLICT,
                                 C::SyncOutcome::INVALIDATED, C::SyncOutcome::CANCELLED}) {
        C::SyncContext context;
        CHECK(C::prepareSync(options(), 99, context));
        const Saved<C::SyncContext> before(context);
        CHECK(!C::advanceSync(context, reason, 98));
        CHECK(before.unchanged(context));
        CHECK(!C::advanceSync(context, C::SyncOutcome::LOCAL_TX, 99));
        CHECK(before.unchanged(context));
        CHECK(C::advanceSync(context, reason, 99));
        CHECK(context.state == C::SyncState::FAILED && context.outcome == reason);
        CHECK(!context.awaitingCompletion && !context.transmissionPossible);
        CHECK(C::advanceSync(context, C::SyncOutcome::INVALIDATED, 100));
        CHECK(context.outcome == reason); // First terminal wins even after another lifecycle change.
        C::SyncContext pending;
        CHECK(C::prepareSync(options(), 99, pending));
        C::SyncWork work;
        CHECK(C::nextSync(pending, 100, work));
        CHECK(C::advanceSync(pending, reason, 110));
        CHECK(pending.outcome == C::SyncOutcome::MISSED_DEADLINE && pending.awaitingCompletion);
        CHECK(C::advanceSync(pending, completion(work), 111));
        CHECK(pending.outcome == C::SyncOutcome::MISSED_DEADLINE && pending.transmissionPossible);
    }
}
} // namespace

int main() {
    codec();
    admission();
    fixedOriginAndCount();
    deadlinesAndRetirement();
    evidence();
    limits();
    interruptions();
    std::puts("SYNC codec and finite caller-clock schedule passed");
}
