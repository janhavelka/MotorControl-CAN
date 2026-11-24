// SPDX-License-Identifier: MIT
// Prompt 57 slice b3: the release (prepareRelease) and its table through the real owner and SDO client,
// with synthetic traffic only. The reports replay the retained bench records the table cites, two CL86-C
// specimens on one firmware; no drive has run this code, and nothing here shows a free shaft.
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/ReferenceRig.h"
namespace {
const uint16_t IN_PLACE = L::MONITOR_POSITION_COMPLETE;
const uint16_t RUNNING = L::MONITOR_RUNNING;
const uint16_t ALARM = L::MONITOR_ALARM;
const uint16_t RELEASED = L::MONITOR_RELEASED;
const uint16_t RELEASED_MONITOR = uint16_t(IN_PLACE | RELEASED); // 0011.
const uint8_t ALL_BLOCKS = 0x1d; // ERRORS, MONITORS, STATUS and FEEDBACK.

// A fresh prerequisite read of this report, 1001 and 603F zero unless error is given.
void acquire(Rig& r, uint16_t status, uint16_t monitor, uint8_t mode, int32_t velocity = 0, uint16_t error = 0) {
    r.statusword = status;
    r.feedbackVelocity = velocity;
    r.acquire(monitor, mode, error, ALL_BLOCKS);
}
// A fresh 2500 read under the rig's reservation, as getControl publishes it.
L::ControlObservation loop(const Rig& r, uint16_t value) {
    L::ControlObservation o;
    o.target = r.current;
    o.loopModeRaw = value;
    o.loopMode = value == 1 ? L::LoopMode::OPEN_LOOP : value == 2 ? L::LoopMode::CLOSED_LOOP : L::LoopMode::UNKNOWN;
    o.window.earliestUs = o.window.latestUs = r.now;
    return o;
}
M::Status attempt(Rig& r, L::ActionContext& action, const L::ControlObservation* control = nullptr,
                  const L::ReleaseExperiment& experiment = L::ReleaseExperiment()) {
    r.begin();
    return L::prepareRelease(action, r.reservation, r.state, control, experiment, r.operation, r.now, r.deadline,
                             r.policy());
}
void prepare(Rig& r, L::ActionContext& action, const L::ControlObservation* control = nullptr,
             const L::ReleaseExperiment& experiment = L::ReleaseExperiment()) {
    CHECK(attempt(r, action, control, experiment));
    r.wire.action = &action;
}
L::ReleaseExperiment experiment(uint16_t word) {
    L::ReleaseExperiment e;
    e.enabled = true;
    e.word = word;
    return e;
}
// Frames from `from` on: exactly `writes` 6040 downloads and the cycle's seven uploads only.
void onlyReleaseTraffic(const Rig& r, unsigned from, unsigned writes) {
    const uint16_t cycle[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061, 0x6064, 0x606c};
    unsigned downloads = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i) {
        const M::Frame& f = r.wire.sent[i];
        CHECK(f.id == 0x601 && f.payloadLength == 8);
        if (isDownload(f)) {
            CHECK(frameIndex(f) == 0x6040 && f.data[0] == 0x2b && f.data[3] == 0);
            ++downloads;
            continue;
        }
        bool listed = false;
        for (uint16_t index : cycle)
            listed |= frameIndex(f) == index;
        CHECK(f.data[0] == 0x40 && listed);
    }
    CHECK(downloads == writes);
}

// Rows 1 to 3: the exact word from the exact report, then settlement on the row's released report only.
// Row 2's settling read gave 606C 1711 beside the release (trial 5991): reported, never a standstill.
void measuredRows() {
    struct Case {
        uint16_t status, monitor;
        uint8_t mode;
        bool openLoop;
        L::ReleaseRow row;
        uint16_t word, afterStatus, afterMonitor;
        int32_t afterVelocity;
    };
    const Case cases[] = {
        {0x8437, IN_PLACE, 1, false, L::ReleaseRow::POWER_UP_IDLE, 0x0006, 0x8031, RELEASED_MONITOR, 0},
        {0x8033, IN_PLACE, 3, true, L::ReleaseRow::SWITCHED_ON_OPEN_LOOP, 0x0000, 0x8050, RELEASED_MONITOR, 1711},
        {0x8010, IN_PLACE, 3, false, L::ReleaseRow::ALARM_CLEARED, 0x0080, 0x8050, RELEASED_MONITOR, 0}};
    for (const Case& k : cases) {
        Rig r;
        r.feedbackPosition = -49;
        acquire(r, k.status, k.monitor, k.mode);
        const L::ControlObservation open = loop(r, 1);
        L::ActionContext action;
        prepare(r, action, k.openLoop ? &open : nullptr);
        const L::ReleaseRecord& m = action.release;
        CHECK(action.actionKind == M::ActionKind::RELEASE && action.state == M::ActionState::ACTIVE &&
              action.retryClass == M::RetryClass::NOT_REPEATABLE && r.wire.sentCount == 9);
        CHECK(m.decision.row == k.row && m.decision.send && m.decision.word == k.word &&
              m.decision.refusal == L::ReleaseRefusal::NONE && m.startStatusword == k.status &&
              m.startMonitor == k.monitor && m.startModeRaw == k.mode && m.loopKnown == k.openLoop);
        const unsigned from = r.wire.sentCount;
        r.command(action, 0x6040, k.word); // 2B 40 60 00 <word>: checked byte for byte.
        CHECK(action.execution == M::ActionExecution::ACKNOWLEDGED && action.phase == L::ActionPhase::OBSERVING);
        // The report before the drive took it is no settlement: it repolls.
        r.cycle(action, k.monitor, -49, 0, k.mode);
        CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING &&
              action.observations == 1);
        r.turn(action.eligibleUs, O::ServiceBudget());
        r.statusword = k.afterStatus;
        r.feedbackVelocity = k.afterVelocity;
        r.cycle(action, k.afterMonitor, -49, 0, k.mode);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.outcome == M::ActionOutcome::OBSERVED &&
              action.completion == M::ActionCompletion::OBSERVED &&
              action.execution == M::ActionExecution::ACKNOWLEDGED && action.observations == 2);
        CHECK(m.settledVelocity == k.afterVelocity && m.settledVelocityNonzero == (k.afterVelocity != 0));
        onlyReleaseTraffic(r, from, 1);
        L::PreparedAction work;
        CHECK(L::nextAction(action, r.reservation, r.current, r.now + 1000, work) && !work.ready);
    }
}

// Row 4 before row 5: 2100 released with low bits 000 or 001, whatever the running bit, mode or 606C, is
// answered already released with nothing sent; the running bit and a nonzero 606C are reported.
void alreadyReleased() {
    struct Case {
        uint16_t status, monitor;
        uint8_t mode;
        int32_t velocity;
    };
    const Case cases[] = {{0x8050, RELEASED_MONITOR, 1, 0},
                          {0x4050, uint16_t(RUNNING | RELEASED), 1, 0},      // Trial 2109's latch.
                          {0xc050, uint16_t(RELEASED_MONITOR | RUNNING), 3, 0}, // Trial 1924's latch.
                          {0x8031, RELEASED_MONITOR, 1, 0},                  // After word 6.
                          {0x8050, RELEASED_MONITOR, 3, 1711}};              // 606C beside the release.
    for (const Case& k : cases) {
        for (unsigned withExperiment = 0; withExperiment < 2; ++withExperiment) {
            Rig r;
            acquire(r, k.status, k.monitor, k.mode, k.velocity);
            const unsigned sent = r.wire.sentCount;
            L::ActionContext action;
            prepare(r, action, nullptr, withExperiment ? experiment(0x0006) : L::ReleaseExperiment());
            const L::ReleaseDecision& d = action.release.decision;
            CHECK(d.row == L::ReleaseRow::ALREADY_RELEASED && !d.send && d.word == 0 &&
                  d.running == ((k.monitor & RUNNING) != 0) && d.velocityNonzero == (k.velocity != 0));
            CHECK(action.state == M::ActionState::SUCCEEDED && action.outcome == M::ActionOutcome::OBSERVED &&
                  action.execution == M::ActionExecution::NOT_TRANSMITTED && !action.effectsPossible &&
                  !action.uncertain && action.commandEvidenceMask == 0);
            L::PreparedAction work;
            CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && !work.ready);
            CHECK(!L::noteActionTransmission(action, r.reservation, r.current, r.now));
            r.turn(r.now + 1000);
            CHECK(r.wire.sentCount == sent); // Exactly zero frames.
        }
    }
}

// Row 5: every other report refused before any frame, with its reason, and the context unchanged.
void refusedBeforeTransmission() {
    struct Case {
        uint16_t status, monitor;
        uint8_t mode;
        int32_t velocity;
        uint16_t error;
        int loopValue; // -1 no 2500 read.
        L::ReleaseRefusal refusal;
    };
    const Case cases[] = {
        {0x8033, IN_PLACE, 3, 0, 0, -1, L::ReleaseRefusal::LOOP},  // Row 2's report with no 2500 read.
        {0x8033, IN_PLACE, 3, 0, 0, 2, L::ReleaseRefusal::LOOP},   // ...in closed loop.
        {0x8033, IN_PLACE, 3, 0, 0, 0, L::ReleaseRefusal::LOOP},
        {0x8033, IN_PLACE, 1, 0, 0, 1, L::ReleaseRefusal::MODE},   // 0x0000 in mode 1.
        {0x8033, IN_PLACE, 3, 5, 0, 1, L::ReleaseRefusal::MOVING}, // ...with 606C nonzero (S21).
        {0x8010, IN_PLACE, 1, 0, 0, -1, L::ReleaseRefusal::MODE},  // 0x0080 outside 8010 in mode 3.
        {0x8010, IN_PLACE, 3, -2, 0, -1, L::ReleaseRefusal::MOVING},
        {0x8437, IN_PLACE, 3, 0, 0, -1, L::ReleaseRefusal::MODE},  // Row 1's pattern in mode 3.
        {0x9437, IN_PLACE, 3, 0, 0, -1, L::ReleaseRefusal::MODE},  // The halted mode 3 report.
        {0x8437, IN_PLACE, 1, 3, 0, -1, L::ReleaseRefusal::MOVING},
        {0x8437, uint16_t(IN_PLACE | RUNNING), 1, 0, 0, -1, L::ReleaseRefusal::RUNNING}, // Moving: a stop.
        {0x8437, RUNNING, 1, 40, 0, -1, L::ReleaseRefusal::RUNNING},
        {0x8437, uint16_t(IN_PLACE | ALARM), 1, 0, 0, -1, L::ReleaseRefusal::ALARM},   // Alarm: the clear.
        {0x8010, IN_PLACE, 3, 0, 0xff05, -1, L::ReleaseRefusal::ALARM},              // 1001 and 603F.
        {0x1437, 0, 1, 0, 0, -1, L::ReleaseRefusal::PATTERN},     // S20's report.
        {0x0037, 0, 1, 0, 0, -1, L::ReleaseRefusal::PATTERN},     // The word-15 report after a latch.
        {0x8432, IN_PLACE, 1, 0, 0, -1, L::ReleaseRefusal::PATTERN},     // Low bits 010: no row.
        {0x8033, RELEASED_MONITOR, 3, 0, 0, 1, L::ReleaseRefusal::RELEASED}, // Released beside low bits 011.
        {0x8037, RELEASED_MONITOR, 1, 0, 0, -1, L::ReleaseRefusal::RELEASED}};
    for (const Case& k : cases) {
        Rig r;
        acquire(r, k.status, k.monitor, k.mode, k.velocity, k.error);
        const L::ControlObservation control = loop(r, uint16_t(k.loopValue < 0 ? 0 : k.loopValue));
        const unsigned sent = r.wire.sentCount;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status s = attempt(r, action, k.loopValue < 0 ? nullptr : &control);
        CHECK(!s && s.code == M::Err::INVALID_CONFIG && s.detail == int32_t(M::ActionError::STATE));
        empty.unchanged(action);
        CHECK(r.wire.sentCount == sent);
        // The table itself names the reason.
        L::ReleaseReport report;
        report.modeRaw = k.mode;
        report.statusword = k.status;
        report.monitor = k.monitor;
        report.errorRegister = k.error ? 1 : 0;
        report.errorCode = k.error;
        report.velocity = k.velocity;
        report.loopKnown = k.loopValue >= 0;
        report.loopModeRaw = uint16_t(k.loopValue < 0 ? 0 : k.loopValue);
        L::ReleaseDecision d;
        CHECK(L::classifyRelease(report, L::ReleaseExperiment(), d));
        CHECK(d.row == L::ReleaseRow::REFUSED && !d.send && d.refusal == k.refusal);
    }
}

// The explicit experiment: 0x0000 or 0x0006 only, from a report outside the measured rows or with another
// word than its row's; it overrides only MOVING, MODE, LOOP and PATTERN, and the result is marked.
void experimentLimits() {
    {   // The word itself: never 0x0080 outside row 3, never bits 0..2 all set, nothing but 0 and 6.
        const uint16_t words[] = {0x0080, 0x0007, 0x000f, 0x000b, 0x0001, 0x0106};
        for (uint16_t word : words) {
            Rig r;
            acquire(r, 0x1437, 0, 1);
            L::ActionContext action;
            const Saved<L::ActionContext> empty(action);
            const M::Status s = attempt(r, action, nullptr, experiment(word));
            CHECK(!s && s.detail == int32_t(M::ActionError::ARGUMENT));
            empty.unchanged(action);
            L::ReleaseDecision d;
            const Saved<L::ReleaseDecision> blank(d);
            CHECK(!L::classifyRelease(L::ReleaseReport(), experiment(word), d));
            blank.unchanged(d);
        }
    }
    struct Case {
        uint16_t status, monitor;
        uint8_t mode;
        int32_t velocity;
        uint16_t word, error;
        L::ReleaseRow row;
    };
    const Case cases[] = {
        {0x8033, IN_PLACE, 3, 0, 0x0006, 0, L::ReleaseRow::EXPERIMENT},       // Word 6 from 8033.
        {0x8033, IN_PLACE, 1, 0, 0x0000, 0, L::ReleaseRow::EXPERIMENT},       // 0x0000 in mode 1.
        {0x8033, IN_PLACE, 3, 1711, 0x0000, 0, L::ReleaseRow::EXPERIMENT},    // S21: 606C, running clear.
        {0x9437, IN_PLACE, 3, 0, 0x0006, 0, L::ReleaseRow::EXPERIMENT},       // The halted mode 3 report.
        {0x0037, 0, 1, 0, 0x0000, 0, L::ReleaseRow::EXPERIMENT},              // 0037/0000.
        {0x8010, IN_PLACE, 3, 0, 0x0000, 0, L::ReleaseRow::EXPERIMENT},       // Row 3's report, another word.
        {0x8437, IN_PLACE, 1, 0, 0x0006, 0, L::ReleaseRow::POWER_UP_IDLE},    // The row's own word: measured.
        {0x8437, uint16_t(IN_PLACE | ALARM), 1, 0, 0x0006, 0, L::ReleaseRow::REFUSED},   // Never an alarm.
        {0x8010, IN_PLACE, 3, 0, 0x0000, 0xff05, L::ReleaseRow::REFUSED},              // Nor a fault code.
        {0x8437, uint16_t(IN_PLACE | RUNNING), 1, 0, 0x0006, 0, L::ReleaseRow::REFUSED}, // Nor running.
        {0x8033, RELEASED_MONITOR, 3, 0, 0x0000, 0, L::ReleaseRow::REFUSED}};          // Nor released.
    for (const Case& k : cases) {
        Rig r;
        r.feedbackPosition = 7;
        acquire(r, k.status, k.monitor, k.mode, k.velocity, k.error);
        const L::ControlObservation open = loop(r, 1);
        L::ActionContext action;
        const M::Status s = attempt(r, action, &open, experiment(k.word));
        if (k.row == L::ReleaseRow::REFUSED) {
            CHECK(!s && s.detail == int32_t(M::ActionError::STATE) && action.state == M::ActionState::EMPTY);
            continue;
        }
        CHECK(s.isOk());
        r.wire.action = &action;
        const L::ReleaseDecision& d = action.release.decision;
        CHECK(d.row == k.row && d.send && d.word == k.word &&
              d.settleLowBits == (k.word == 0x0006 ? 1 : k.word == 0x0080 ? 0 : 0));
        const unsigned from = r.wire.sentCount;
        r.command(action, 0x6040, k.word);
        r.statusword = uint16_t(0x8050 | (k.word == 0x0006 ? 0x0001 : 0)); // The p12 table's low bits.
        r.feedbackVelocity = 0;
        r.cycle(action, RELEASED_MONITOR, 7, 0, k.mode);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.execution == M::ActionExecution::ACKNOWLEDGED);
        onlyReleaseTraffic(r, from, 1);
    }
}

// A cycle outside the scope ends the release with that cycle kept and nothing written again: another mode,
// an alarm, running newly set. Until the deadline a report still short of the release repolls.
void cycleScopeAndDeadline() {
    struct Case {
        uint16_t monitor;
        uint8_t mode;
    };
    const Case cases[] = {{IN_PLACE, 3}, {uint16_t(RELEASED_MONITOR | ALARM), 1}, {uint16_t(IN_PLACE | RUNNING), 1}};
    for (const Case& k : cases) {
        Rig r;
        acquire(r, 0x8437, IN_PLACE, 1);
        L::ActionContext action;
        prepare(r, action);
        const unsigned from = r.wire.sentCount;
        r.command(action, 0x6040, 0x0006);
        r.statusword = 0x8031;
        r.cycle(action, k.monitor, 123, 0, k.mode);
        CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::OBSERVATION_SCOPE &&
              action.execution == M::ActionExecution::ACKNOWLEDGED &&
              action.completion == M::ActionCompletion::NOT_OBSERVED && action.uncertain);
        onlyReleaseTraffic(r, from, 1);
    }
    Rig r;
    acquire(r, 0x8437, IN_PLACE, 1);
    L::ActionContext action;
    prepare(r, action);
    const unsigned from = r.wire.sentCount;
    r.command(action, 0x6040, 0x0006);
    r.cycle(action, IN_PLACE, 123, 0, 1); // Still 8437/0001.
    CHECK(action.phase == L::ActionPhase::WAITING);
    r.turn(r.deadline, O::ServiceBudget());
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::DEADLINE &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain);
    onlyReleaseTraffic(r, from, 1);
}

// The word fails at each boundary: a lost reply stays UNKNOWN and uncertain, an abort REJECTED, and the word
// is never sent again, also after a late reply.
void wordFailureAtEveryStage() {
    for (Stage stage : STAGES) {
        Rig r;
        r.keepQuarantine();
        acquire(r, 0x8437, IN_PLACE, 1);
        L::ActionContext action;
        prepare(r, action);
        const unsigned from = r.wire.sentCount;
        const Rig::Failure f = r.failCommand(action, stage);
        CHECK(action.state == M::ActionState::FAILED && action.completion == M::ActionCompletion::NOT_OBSERVED);
        const bool acked = acknowledged(stage);
        CHECK(action.outcome == outcomeAt(stage));
        CHECK(action.execution == (!sent(stage)              ? M::ActionExecution::NOT_TRANSMITTED
                                   : stage == Stage::ABORTED ? M::ActionExecution::REJECTED
                                   : acked                   ? M::ActionExecution::ACKNOWLEDGED
                                                             : M::ActionExecution::UNKNOWN));
        CHECK(action.uncertain == sent(stage) && action.effectsPossible == sent(stage));
        CHECK(r.services.unresolved(0) == ambiguous(stage));
        onlyReleaseTraffic(r, from, sent(stage) ? 1 : 0);
        if (f.admitted)
            r.lateReplyChangesNothing(action, f);
        onlyReleaseTraffic(r, from, sent(stage) ? 1 : 0); // Never resent.
    }
}

// Inputs: budgets, overlap, a stale 2500 read, and the context check of a hand-edited release.
void inputsAndShape() {
    Rig r;
    acquire(r, 0x8033, IN_PLACE, 3);
    L::ControlObservation open = loop(r, 1);
    L::ActionContext action;
    const Saved<L::ActionContext> empty(action);
    r.begin();
    L::ActionOptions options = r.policy();
    options.maximumAgeUs = 7 * 200 - 1;
    CHECK(L::prepareRelease(action, r.reservation, r.state, &open, L::ReleaseExperiment(), r.operation, r.now,
                            r.deadline, options).detail == int32_t(M::ActionError::ARGUMENT));
    CHECK(!L::prepareRelease(action, r.reservation, r.state, &open, L::ReleaseExperiment(), 0, r.now, r.deadline,
                             r.policy()));
    L::ControlObservation other = open;
    ++other.target.nodeGeneration; // Another node epoch.
    CHECK(L::prepareRelease(action, r.reservation, r.state, &other, L::ReleaseExperiment(), r.operation, r.now,
                            r.deadline, r.policy()).detail == int32_t(M::ActionError::STATE));
    other = open;
    other.window.earliestUs = r.reservation.acquiredUs; // Older than the allowed age at a later time.
    CHECK(L::prepareRelease(action, r.reservation, r.state, &other, L::ReleaseExperiment(), r.operation,
                            r.now + 1400, r.deadline, r.policy()).detail == int32_t(M::ActionError::STATE));
    empty.unchanged(action);
    static L::ActionContext aliasing;
    aliasing.reservation = r.reservation;
    CHECK(L::prepareRelease(aliasing, aliasing.reservation, r.state, &open, L::ReleaseExperiment(), r.operation,
                            r.now, r.deadline, r.policy()).detail == int32_t(M::ActionError::OVERLAP));
    prepare(r, action, &open);
    L::PreparedAction work;
    L::ActionContext edited = action;
    edited.release.decision.word = 0x000f; // A word with bits 0..2 all set.
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    edited = action;
    edited.release.decision.row = L::ReleaseRow::REFUSED;
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    edited = action;
    edited.actionKind = M::ActionKind::SELECT_MODE; // Another kind holding a release record.
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && work.ready &&
          work.transfer.index == 0x6040 && work.transfer.size == 2 && work.transfer.data[0] == 0 &&
          work.transfer.data[1] == 0);
}
} // namespace
int main() {
    measuredRows();
    alreadyReleased();
    refusedBeforeTransmission();
    experimentLimits();
    cycleScopeAndDeadline();
    wordFailureAtEveryStage();
    inputsAndShape();
    std::printf("release: %u checks passed\n", checks);
}
