// SPDX-License-Identifier: MIT
// Prompt 57 slice b1: the typed mode select (prepareModeSelect) through the real owner and SDO client,
// with synthetic traffic only. The drive reports replay retained bench records of two CL86-C specimens on
// one firmware; no drive has run this code.
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
#include "support/Identity.h"
namespace {
const uint16_t IN_PLACE = L::MONITOR_POSITION_COMPLETE;
const uint16_t RUNNING = L::MONITOR_RUNNING;
const uint16_t ALARM = L::MONITOR_ALARM;
const uint16_t RELEASED = L::MONITOR_RELEASED;
// 2100 and 6041 as the specimens reported them: the power-up report and the same drive after 6060=1
// (packet 15 P2, 23 records), the released standstill a quick stop leaves (8050/0011), the latch of a
// moving quick stop (C050/0015, trial 1924; 4050/0014, trial 2109), the report after word 6 from a
// release (8031/0011), the enabled report in mode 1 (8437/0001), the switched-on report a velocity enable
// refused at word 7 left (8033/0001, trial 5976) and the report after a 2301 clear (8010/0001, trial 5323).
const uint16_t POWER_UP_STATUS = 0x8037, POWER_UP_MONITOR = IN_PLACE;
const uint16_t MODE_ONE_STATUS = 0x8437;
const uint16_t RELEASED_STATUS = 0x8050, RELEASED_MONITOR = uint16_t(IN_PLACE | RELEASED);
const uint16_t LATCHED_STATUS = 0xc050, LATCHED_MONITOR = uint16_t(IN_PLACE | RUNNING | RELEASED);
const uint16_t LATCHED_ONE_STATUS = 0x4050, LATCHED_ONE_MONITOR = uint16_t(RUNNING | RELEASED);
const uint16_t WORD_SIX_STATUS = 0x8031;
const uint16_t SWITCHED_ON_STATUS = 0x8033, CLEARED_STATUS = 0x8010;
const uint8_t ALL_BLOCKS = 0x1d; // ERRORS, MONITORS, STATUS and FEEDBACK; no 1003 read.

// A current identity of this boot with the measured values (support/Identity.h): the fake answers no
// identity read.
L::IdentityObservation identityOf(const Rig& r) { return TestIdentity::measured(r.reservation.target, r.now); }
M::Status attempt(Rig& r, L::ActionContext& action, uint8_t mode) {
    r.begin();
    return L::prepareModeSelect(action, r.reservation, r.state, identityOf(r), mode, r.operation, r.now, r.deadline,
                                r.policy());
}
// A fresh prerequisite read of this report, with 1001 and 603F zero.
void acquire(Rig& r, uint16_t status, uint16_t monitor, uint8_t mode, uint8_t mask = ALL_BLOCKS) {
    r.statusword = status;
    r.acquire(monitor, mode, 0, mask);
}
void prepare(Rig& r, L::ActionContext& action, uint8_t mode) {
    CHECK(attempt(r, action, mode));
    r.wire.action = &action;
}
// The one 6060 download, acknowledged: 2F 60 60 00 <mode> 00 00 00 to node 1, nothing else.
void select(Rig& r, L::ActionContext& action, uint8_t mode) {
    const L::PreparedAction work = r.next(action);
    CHECK(work.ready && work.transfer.direction == C::SdoDirection::DOWNLOAD &&
          work.transfer.index == 0x6060 && work.transfer.subindex == 0 && work.transfer.size == 1 &&
          work.transfer.data[0] == mode && work.transfer.step == 1 &&
          work.transfer.operation == action.operation);
    const unsigned sent = r.wire.sentCount;
    r.field(action);
    CHECK(r.wire.sentCount == sent + 1);
    const uint8_t expected[8] = {0x2f, 0x60, 0x60, 0, mode, 0, 0, 0};
    CHECK(r.wire.sent[sent].id == 0x601 && r.wire.sent[sent].payloadLength == 8 &&
          !std::memcmp(r.wire.sent[sent].data, expected, sizeof expected));
    CHECK(action.execution == M::ActionExecution::ACKNOWLEDGED &&
          action.commandEvidenceMask == 1 && action.phase == L::ActionPhase::OBSERVING);
}
// Frames sent from `from` on: one 6060 download at most (exactly `writes`), and uploads of the cycle's
// seven fields only. Never a 6060 upload, a 6061 download, a controlword, a target or a second 6060.
void onlyModeTraffic(const Rig& r, unsigned from, unsigned writes) {
    const uint16_t cycle[] = {0x2100, 0x2101, 0x2102, 0x6041, 0x6061, 0x6064, 0x606c};
    unsigned downloads = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i) {
        const M::Frame& f = r.wire.sent[i];
        CHECK(f.id == 0x601 && f.payloadLength == 8);
        if (isDownload(f)) {
            CHECK(frameIndex(f) == 0x6060 && f.data[0] == 0x2f && f.data[3] == 0);
            ++downloads;
            continue;
        }
        CHECK(f.data[0] == 0x40);
        bool listed = false;
        for (uint16_t index : cycle)
            listed |= frameIndex(f) == index;
        CHECK(listed);
    }
    CHECK(downloads == writes);
}
void succeeded(const L::ActionContext& action, uint8_t mode, uint32_t observations) {
    CHECK(action.state == M::ActionState::SUCCEEDED && action.outcome == M::ActionOutcome::OBSERVED &&
          action.completion == M::ActionCompletion::OBSERVED &&
          action.execution == M::ActionExecution::ACKNOWLEDGED && action.status.isOk());
    CHECK(action.statusObservation.modeRaw == mode && action.observations == observations &&
          action.retryClass == M::RetryClass::NOT_REPEATABLE && !action.valueOneMayRemain);
}

// Each admitted start, value for value: the power-up report to mode 1 (packet 15 P2), released mode 1 to
// 3 (trial 917, node 2 at -49), released and latched mode 3 to 1 (records 2101 and 2221), the report word 6
// leaves to mode 3, and the latch of a moving mode 1 quick stop to mode 6, a documented code never written
// on the bench. One write, then cycles until 6061 reads the requested mode.
void admittedStarts() {
    struct Case {
        uint16_t status, monitor, afterStatus;
        uint8_t mode, requested;
        int32_t position;
        L::ModeSelectStart start;
    };
    const Case cases[] = {
        {POWER_UP_STATUS, POWER_UP_MONITOR, MODE_ONE_STATUS, 0, 1, 1180, L::ModeSelectStart::POWER_UP},
        {RELEASED_STATUS, RELEASED_MONITOR, RELEASED_STATUS, 1, 3, -49, L::ModeSelectStart::NOT_ENABLED},
        {RELEASED_STATUS, RELEASED_MONITOR, RELEASED_STATUS, 3, 1, 1522, L::ModeSelectStart::NOT_ENABLED},
        {LATCHED_STATUS, LATCHED_MONITOR, LATCHED_STATUS, 3, 1, 33743, L::ModeSelectStart::NOT_ENABLED},
        {WORD_SIX_STATUS, RELEASED_MONITOR, WORD_SIX_STATUS, 1, 3, 15257, L::ModeSelectStart::NOT_ENABLED},
        {LATCHED_ONE_STATUS, LATCHED_ONE_MONITOR, LATCHED_ONE_STATUS, 1, 6, -3522,
         L::ModeSelectStart::NOT_ENABLED}};
    for (const Case& item : cases) {
        Rig r;
        r.feedbackPosition = item.position;
        acquire(r, item.status, item.monitor, item.mode);
        L::ActionContext action;
        prepare(r, action, item.requested);
        const L::ModeSelectRecord& m = action.modeSelect;
        CHECK(action.actionKind == M::ActionKind::SELECT_MODE && action.state == M::ActionState::ACTIVE &&
              action.phase == L::ActionPhase::COMMAND && action.execution == M::ActionExecution::NOT_TRANSMITTED &&
              action.retryClass == M::RetryClass::NOT_REPEATABLE && r.wire.sentCount == 9);
        CHECK(m.start == item.start && m.requestedModeRaw == item.requested && m.startModeRaw == item.mode &&
              m.startStatusword == item.status && m.startMonitor == item.monitor &&
              m.startPosition == item.position);
        // No other action's record is set, and no stop, clear, enable or counter state.
        CHECK(!action.incident.present && !action.baseline.present && !action.enable.evidence.trial &&
              !action.standstillObserving && action.clearMethod == L::AlarmClearMethod::OBJECT_2301);
        const unsigned from = r.wire.sentCount;
        select(r, action, item.requested);
        r.statusword = item.afterStatus;
        r.cycle(action, item.monitor, item.position, 0, item.requested);
        succeeded(action, item.requested, 1);
        CHECK(action.monitorObservation.monitor == item.monitor &&
              action.statusObservation.statusword == item.afterStatus &&
              action.extraObservation.actualPosition == item.position &&
              action.extraObservation.actualVelocity == 0);
        onlyModeTraffic(r, from, 1);
        CHECK(r.uploadsOf(from, 0x6061) == 1 && r.uploadsOf(from, 0x6060) == 0);
        // Terminal: nothing more is yielded, ever.
        L::PreparedAction work;
        CHECK(L::nextAction(action, r.reservation, r.current, r.now + 1000, work) && !work.ready);
    }
}

// The library's admission is the fresh report alone: an enabled drive, a report that is neither released
// nor the power-up report, the power-up report to another mode, and every other prerequisite refuse
// before any frame, with the context unchanged.
void refusedBeforeTransmission() {
    struct Case {
        uint16_t status, monitor;
        uint8_t mode, requested, mask;
        int32_t velocity;
        uint16_t error;
    };
    const Case cases[] = {
        {MODE_ONE_STATUS, POWER_UP_MONITOR, 1, 3, ALL_BLOCKS, 0, 0}, // Enabled in mode 1 (Q11).
        {MODE_ONE_STATUS, POWER_UP_MONITOR, 1, 6, ALL_BLOCKS, 0, 0},
        {0x8437, POWER_UP_MONITOR, 3, 1, ALL_BLOCKS, 0, 0},           // Enabled in mode 3.
        {POWER_UP_STATUS, POWER_UP_MONITOR, 1, 3, ALL_BLOCKS, 0, 0},  // The power-up pattern, not in mode 0.
        {POWER_UP_STATUS, POWER_UP_MONITOR, 0, 3, ALL_BLOCKS, 0, 0},  // Power-up to 3: never measured.
        {POWER_UP_STATUS, POWER_UP_MONITOR, 0, 6, ALL_BLOCKS, 0, 0},  // Power-up to 6.
        {0x8027, POWER_UP_MONITOR, 0, 1, ALL_BLOCKS, 0, 0},           // Low bits 111, not exactly 8037.
        {POWER_UP_STATUS, uint16_t(IN_PLACE | RUNNING), 0, 1, ALL_BLOCKS, 0, 0}, // Not exactly 0001.
        {POWER_UP_STATUS, 0, 0, 1, ALL_BLOCKS, 0, 0},
        {SWITCHED_ON_STATUS, POWER_UP_MONITOR, 3, 1, ALL_BLOCKS, 0, 0}, // S21: switched on, not released.
        {CLEARED_STATUS, POWER_UP_MONITOR, 3, 1, ALL_BLOCKS, 0, 0},     // After the 2301 clear.
        {0x8033, RELEASED_MONITOR, 1, 3, ALL_BLOCKS, 0, 0},             // Released beside low bits 011.
        {0x8037, RELEASED_MONITOR, 1, 3, ALL_BLOCKS, 0, 0},             // Released beside low bits 111.
        {POWER_UP_STATUS, POWER_UP_MONITOR, 1, 1, ALL_BLOCKS, 0, 0},    // In the mode, but enabled.
        {MODE_ONE_STATUS, POWER_UP_MONITOR, 1, 1, ALL_BLOCKS, 0, 0},
        {RELEASED_STATUS, uint16_t(RELEASED_MONITOR | ALARM), 1, 1, ALL_BLOCKS, 0, 0}, // In it, alarmed.
        {RELEASED_STATUS, RELEASED_MONITOR, 1, 1, ALL_BLOCKS, 1, 0},    // In it, 606C moving.
        {RELEASED_STATUS, uint16_t(RELEASED_MONITOR | ALARM), 1, 3, ALL_BLOCKS, 0, 0}, // Alarm.
        {RELEASED_STATUS, RELEASED_MONITOR, 1, 3, ALL_BLOCKS, 0, 0xff05},             // 1001, 603F.
        {RELEASED_STATUS, RELEASED_MONITOR, 1, 3, ALL_BLOCKS, 1, 0},                  // 606C moving.
        {LATCHED_STATUS, LATCHED_MONITOR, 3, 1, ALL_BLOCKS, -2, 0},                   // Latch with 606C.
        {RELEASED_STATUS, RELEASED_MONITOR, 1, 3, 0x1c, 0, 0},  // No ERRORS block.
        {RELEASED_STATUS, RELEASED_MONITOR, 1, 3, 0x0d, 0, 0},  // No FEEDBACK block.
        {RELEASED_STATUS, RELEASED_MONITOR, 1, 3, 0x19, 0, 0},  // No MONITORS block.
        {RELEASED_STATUS, RELEASED_MONITOR, 1, 3, 0x15, 0, 0},  // No STATUS block.
        // Fix round c, item 4: running only in the exact quick-stop latch, and the released standstill's shape.
        {RELEASED_STATUS, LATCHED_MONITOR, 3, 1, ALL_BLOCKS, 0, 0},     // 0015 with 8050: no bit 14.
        {0x0001, LATCHED_ONE_MONITOR, 1, 3, ALL_BLOCKS, 0, 0},          // 0014 with 0001.
        {0x8051, LATCHED_ONE_MONITOR, 1, 3, ALL_BLOCKS, 0, 0},          // 0014 with low bits 001.
        {LATCHED_STATUS, uint16_t(LATCHED_MONITOR | 0x0020), 3, 1, ALL_BLOCKS, 0, 0}, // Latch with a soft limit.
        {RELEASED_STATUS, uint16_t(RELEASED_MONITOR | 0x0080), 1, 3, ALL_BLOCKS, 0, 0}, // 2100 bit 7.
        {RELEASED_STATUS, uint16_t(RELEASED_MONITOR | L::MONITOR_POSITIVE_SOFT_LIMIT), 1, 3, ALL_BLOCKS, 0, 0},
        {RELEASED_STATUS, uint16_t(RELEASED_MONITOR | L::MONITOR_NEGATIVE_SOFT_LIMIT), 1, 3, ALL_BLOCKS, 0, 0},
        {0x0000, RELEASED, 1, 3, ALL_BLOCKS, 0, 0},                       // 0010 with 0000.
        {0x8000, RELEASED, 1, 1, ALL_BLOCKS, 0, 0}}; // Already in the mode, yet not the released standstill.
    for (const Case& item : cases) {
        Rig r;
        r.feedbackVelocity = item.velocity;
        r.statusword = item.status;
        r.acquire(item.monitor, item.mode, item.error, item.mask);
        const unsigned sent = r.wire.sentCount;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status status = attempt(r, action, item.requested);
        CHECK(!status && status.code == M::Err::INVALID_CONFIG &&
              status.detail == int32_t(M::ActionError::STATE));
        empty.unchanged(action);
        CHECK(r.wire.sentCount == sent);
    }
    // The enabled report names Q11 in its reason.
    Rig r;
    acquire(r, MODE_ONE_STATUS, POWER_UP_MONITOR, 1);
    L::ActionContext action;
    const M::Status enabled = attempt(r, action, 3);
    CHECK(!enabled && std::strstr(enabled.msg, "Q11"));
}

// Every code but 1, 3 and 6 is UNSUPPORTED before any other check, even with no read at all and a
// refused reservation: the undocumented codes stay in the D17 probe, and 0 is no operating mode.
void undocumentedCodes() {
    for (unsigned code = 0; code < 256; ++code) {
        if (code == 1 || code == 3 || code == 6)
            continue;
        Rig r;
        acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status status = attempt(r, action, uint8_t(code));
        CHECK(!status && status.code == M::Err::UNSUPPORTED &&
              status.detail == int32_t(M::ActionError::ARGUMENT));
        empty.unchanged(action);
        L::ReadContext none;
        L::AxisReservation unbound;
        CHECK(L::prepareModeSelect(action, unbound, none, L::IdentityObservation(), uint8_t(code), 0, 5, 5, L::ActionOptions()).code ==
              M::Err::UNSUPPORTED);
        empty.unchanged(action);
    }
    // A documented code with the same bad inputs is refused for those inputs instead.
    L::ActionContext action;
    L::ReadContext none;
    L::AxisReservation unbound;
    const M::Status bad = L::prepareModeSelect(action, unbound, none, L::IdentityObservation(), 3, 0, 5, 5, L::ActionOptions());
    CHECK(!bad && bad.code == M::Err::INVALID_CONFIG && bad.detail == int32_t(M::ActionError::ARGUMENT));
}

// Budgets and inputs: a cycle reads seven fields, so maximumAgeUs must cover them; overlap is refused.
void budgetsAndOverlap() {
    Rig r;
    acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
    L::ActionContext action;
    const Saved<L::ActionContext> empty(action);
    r.begin();
    L::ActionOptions options = r.policy();
    options.maximumAgeUs = 7 * 200 - 1;
    M::Status s = L::prepareModeSelect(action, r.reservation, r.state, identityOf(r), 3, r.operation, r.now, r.deadline, options);
    CHECK(!s && s.detail == int32_t(M::ActionError::ARGUMENT));
    options = r.policy();
    options.observationIntervalUs = 0;
    CHECK(!L::prepareModeSelect(action, r.reservation, r.state, identityOf(r), 3, r.operation, r.now, r.deadline, options));
    options = r.policy();
    options.service.identityWidths = L::IdentityWidths::MEASURED_U8;
    CHECK(!L::prepareModeSelect(action, r.reservation, r.state, identityOf(r), 3, r.operation, r.now, r.deadline, options));
    CHECK(!L::prepareModeSelect(action, r.reservation, r.state, identityOf(r), 3, 0, r.now, r.deadline, r.policy()));
    CHECK(!L::prepareModeSelect(action, r.reservation, r.state, identityOf(r), 3, r.operation, r.now, r.now, r.policy()));
    empty.unchanged(action);
    // The read aged past maximumAgeUs before preparation.
    s = L::prepareModeSelect(action, r.reservation, r.state, identityOf(r), 3, r.operation, r.now + 1400, r.deadline,
                             r.policy());
    CHECK(!s && s.detail == int32_t(M::ActionError::STATE));
    empty.unchanged(action);
    // Overlap with each borrowed input.
    static L::ActionContext aliasing;
    aliasing.reservation = r.reservation;
    aliasing.read = r.state;
    aliasing.options = r.policy();
    const Saved<L::ActionContext> aliasingSaved(aliasing);
    CHECK(L::prepareModeSelect(aliasing, aliasing.reservation, r.state, identityOf(r), 3, r.operation, r.now, r.deadline,
                               r.policy()).detail == int32_t(M::ActionError::OVERLAP));
    CHECK(L::prepareModeSelect(aliasing, r.reservation, aliasing.read, identityOf(r), 3, r.operation, r.now, r.deadline,
                               r.policy()).detail == int32_t(M::ActionError::OVERLAP));
    CHECK(L::prepareModeSelect(aliasing, r.reservation, r.state, identityOf(r), 3, r.operation, r.now, r.deadline,
                               aliasing.options).detail == int32_t(M::ActionError::OVERLAP));
    // Overlap is judged before the code.
    CHECK(L::prepareModeSelect(aliasing, aliasing.reservation, r.state, identityOf(r), 2, r.operation, r.now, r.deadline,
                               r.policy()).detail == int32_t(M::ActionError::OVERLAP));
    aliasingSaved.unchanged(aliasing);
    // A reservation for another configuration epoch is not this read's.
    L::AxisReservation stale = r.reservation;
    ++stale.target.channel.binding.configurationGeneration;
    CHECK(!L::prepareModeSelect(action, stale, r.state, identityOf(r), 3, r.operation, r.now, r.deadline, r.policy()));
    empty.unchanged(action);
}

// Settlement waits for 6061: cycles that still report the start mode repoll at the interval, and the first
// cycle in scope that reports the requested mode completes it.
void settlesOnModeReport() {
    Rig r;
    r.feedbackPosition = -49;
    acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
    L::ActionContext action;
    prepare(r, action, 3);
    const unsigned from = r.wire.sentCount;
    select(r, action, 3);
    for (unsigned cycle = 1; cycle <= 3; ++cycle) {
        r.cycle(action, RELEASED_MONITOR, -49, 0, 1); // The drive still reports mode 1.
        CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING &&
              action.observations == cycle && action.completion == M::ActionCompletion::NOT_OBSERVED);
        CHECK(!r.next(action).ready); // Held until the interval.
        r.turn(action.eligibleUs, O::ServiceBudget());
    }
    r.cycle(action, RELEASED_MONITOR, -49, 0, 3);
    succeeded(action, 3, 4);
    onlyModeTraffic(r, from, 1);
    CHECK(r.uploadsOf(from, 0x6061) == 4);
}

// A cycle outside the scope ends the action with that cycle kept, the acknowledgement kept apart from the
// missing completion, and nothing written again: another 6061, a lost release, an enabled pattern, an
// alarm, running, a moving 606C or a changed 6064.
void cycleOutsideScope() {
    struct Case {
        uint16_t monitor, status;
        uint8_t mode;
        int32_t position, velocity;
        bool modeMessage;
    };
    const Case cases[] = {
        {RELEASED_MONITOR, RELEASED_STATUS, 6, -49, 0, true},  // 6061 mismatch: neither 1 nor 3.
        {RELEASED_MONITOR, RELEASED_STATUS, 0, -49, 0, true},
        {POWER_UP_MONITOR, RELEASED_STATUS, 3, -49, 0, false}, // The release went.
        {RELEASED_MONITOR, 0x8033, 3, -49, 0, false},          // Switched on.
        {RELEASED_MONITOR, 0x8037, 3, -49, 0, false},
        {uint16_t(RELEASED_MONITOR | ALARM), RELEASED_STATUS, 3, -49, 0, false},
        {uint16_t(RELEASED_MONITOR | RUNNING), RELEASED_STATUS, 3, -49, 0, false}, // Running, never admitted.
        {RELEASED_MONITOR, RELEASED_STATUS, 3, -49, 5, false},
        {RELEASED_MONITOR, RELEASED_STATUS, 3, -48, 0, false}};
    for (const Case& item : cases) {
        Rig r;
        r.feedbackPosition = -49;
        acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
        L::ActionContext action;
        prepare(r, action, 3);
        const unsigned from = r.wire.sentCount;
        select(r, action, 3);
        r.statusword = item.status;
        r.feedbackVelocity = item.velocity;
        r.cycle(action, item.monitor, item.position, 0, item.mode);
        CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::OBSERVATION_SCOPE &&
              action.completion == M::ActionCompletion::NOT_OBSERVED &&
              action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain && action.effectsPossible);
        CHECK(item.modeMessage == (std::strstr(action.status.msg, "6061") != nullptr));
        CHECK(action.statusObservation.modeRaw == item.mode && action.monitorObservation.monitor == item.monitor &&
              action.observations == 1);
        onlyModeTraffic(r, from, 1);
        L::PreparedAction work;
        CHECK(L::nextAction(action, r.reservation, r.current, r.now + 1000, work) && !work.ready);
    }
    // After the power-up start the drive must keep its own report: a release there is outside the scope.
    Rig r;
    acquire(r, POWER_UP_STATUS, POWER_UP_MONITOR, 0);
    L::ActionContext action;
    prepare(r, action, 1);
    select(r, action, 1);
    r.statusword = 0x8031;
    r.cycle(action, RELEASED_MONITOR, 123, 0, 1);
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
}

// A cycle that keeps reporting the start mode repolls until the deadline, which ends it without completion;
// the acknowledged write is never sent again.
void deadlineWithoutModeReport() {
    Rig r;
    acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
    L::ActionContext action;
    prepare(r, action, 3);
    const unsigned from = r.wire.sentCount;
    select(r, action, 3);
    r.cycle(action, RELEASED_MONITOR, 123, 0, 1);
    CHECK(action.phase == L::ActionPhase::WAITING);
    r.turn(r.deadline, O::ServiceBudget());
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::DEADLINE &&
          action.execution == M::ActionExecution::ACKNOWLEDGED &&
          action.completion == M::ActionCompletion::NOT_OBSERVED && action.uncertain);
    onlyModeTraffic(r, from, 1);
}

// The 6060 fails at each boundary: execution keeps exactly what is known, a lost reply stays UNKNOWN and
// uncertain, an abort is REJECTED, and nothing is resent, also after a late reply.
void writeFailureAtEveryStage() {
    for (Stage stage : STAGES) {
        Rig r;
        r.keepQuarantine(); // The matrix asserts the quarantine and exact traffic.
        acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
        L::ActionContext action;
        prepare(r, action, 3);
        const unsigned from = r.wire.sentCount;
        const Rig::Failure f = r.failCommand(action, stage);
        CHECK(action.state == M::ActionState::FAILED && action.completion == M::ActionCompletion::NOT_OBSERVED);
        const bool acked = acknowledged(stage);
        CHECK(acked == (f.admitted && f.terminal.outcome == C::SdoOutcome::SUCCESS));
        CHECK(action.outcome == outcomeAt(stage));
        CHECK(action.commandEvidenceMask == (f.admitted ? 1 : 0));
        CHECK(action.execution == (!sent(stage)              ? M::ActionExecution::NOT_TRANSMITTED
                                   : stage == Stage::ABORTED ? M::ActionExecution::REJECTED
                                   : acked                   ? M::ActionExecution::ACKNOWLEDGED
                                                             : M::ActionExecution::UNKNOWN));
        CHECK(action.uncertain == sent(stage) && action.effectsPossible == sent(stage));
        CHECK(!action.valueOneMayRemain && action.observations == 0);
        CHECK(r.services.unresolved(0) == ambiguous(stage));
        CHECK(r.wire.sentCount == f.sent + (sent(stage) ? 1u : 0u));
        onlyModeTraffic(r, from, sent(stage) ? 1 : 0);
        if (f.admitted)
            r.lateReplyChangesNothing(action, f);
        onlyModeTraffic(r, from, sent(stage) ? 1 : 0); // Never resent.
    }
}

// A request for the mode a fresh 6061 already reports, from a not-enabled report, is answered as already
// selected: terminal SUCCEEDED and OBSERVED at preparation, execution NOT_TRANSMITTED, alreadySelected set,
// and exactly zero frames, then or later. As the release table's already-released row answers.
void alreadySelected() {
    struct Case {
        uint16_t status, monitor;
        uint8_t mode;
    };
    const Case cases[] = {{RELEASED_STATUS, RELEASED_MONITOR, 1}, {RELEASED_STATUS, RELEASED_MONITOR, 3},
                          {LATCHED_STATUS, LATCHED_MONITOR, 3}, {WORD_SIX_STATUS, RELEASED_MONITOR, 6}};
    for (const Case& item : cases) {
        Rig r;
        acquire(r, item.status, item.monitor, item.mode);
        const unsigned sent = r.wire.sentCount;
        L::ActionContext action;
        prepare(r, action, item.mode);
        CHECK(action.actionKind == M::ActionKind::SELECT_MODE && action.modeSelect.alreadySelected &&
              action.modeSelect.start == L::ModeSelectStart::NOT_ENABLED &&
              action.modeSelect.requestedModeRaw == item.mode && action.modeSelect.startModeRaw == item.mode);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.outcome == M::ActionOutcome::OBSERVED &&
              action.completion == M::ActionCompletion::OBSERVED &&
              action.execution == M::ActionExecution::NOT_TRANSMITTED && action.status.isOk() &&
              !action.effectsPossible && !action.uncertain && action.commandEvidenceMask == 0 &&
              action.observations == 0 && action.retryClass == M::RetryClass::NOT_REPEATABLE);
        CHECK(action.statusObservation.modeRaw == item.mode && action.statusObservation.statusword == item.status);
        const Saved<L::ActionContext> done(action);
        L::PreparedAction work;
        CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && !work.ready);
        CHECK(!L::noteActionTransmission(action, r.reservation, r.current, r.now));
        CHECK(!L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
        done.unchanged(action);
        r.turn(r.now + 1000);
        CHECK(r.wire.sentCount == sent); // Exactly zero frames.
    }
    // The flag is never valid on work in progress, nor on another kind.
    Rig r;
    acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
    L::ActionContext action;
    prepare(r, action, 3);
    L::PreparedAction work;
    L::ActionContext edited = action;
    edited.modeSelect.alreadySelected = true;
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    edited.modeSelect.requestedModeRaw = 1; // Equal to the start mode, yet still active.
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
}
// The context check refuses a hand-edited mode select before it yields anything.
void shapeIsChecked() {
    Rig r;
    acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
    L::ActionContext action;
    prepare(r, action, 3);
    L::PreparedAction work;
    const uint8_t badModes[] = {0, 2, 1};
    for (uint8_t bad : badModes) {
        L::ActionContext edited = action;
        edited.modeSelect.requestedModeRaw = bad; // Undocumented, or equal to the start mode.
        CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    }
    L::ActionContext edited = action;
    edited.modeSelect.start = L::ModeSelectStart::POWER_UP; // Not the power-up report.
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    edited = action;
    edited.modeSelect.startMonitor = POWER_UP_MONITOR; // Not released.
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    edited = action;
    edited.actionKind = M::ActionKind::CLEAR_ALARM; // Another kind holding a mode select record.
    CHECK(!L::nextAction(edited, r.reservation, r.current, r.now, work));
    CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && work.ready &&
          work.transfer.index == 0x6060);
}
// Prompt 57 identity rule, as enable's (fix round c, item 1): product fields (1000, 1018:01, 1018:02, 2000)
// that differ from measuredIdentity(), or an identity of another boot, refuse before any TX with
// UNSUPPORTED and ActionError::TARGET; a firmware field (1009, 100A, 1018:03, 2001) that differs admits
// the write and flags it. A state refusal names itself first, and an already selected mode, which sends
// nothing, is answered without a current identity.
void identityRule() {
    enum Edit { PRODUCT_2000, PRODUCT_1000, PRODUCT_VENDOR, PRODUCT_CODE, DOCUMENTED_2000, OTHER_BOOT, NONE_READ };
    for (int edit = PRODUCT_2000; edit <= NONE_READ; ++edit) {
        Rig r;
        acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
        r.begin();
        L::IdentityObservation id = identityOf(r);
        if (edit == PRODUCT_2000)
            ++id.modelCode;
        else if (edit == PRODUCT_1000)
            ++id.deviceType;
        else if (edit == PRODUCT_VENDOR)
            ++id.vendorId;
        else if (edit == PRODUCT_CODE)
            ++id.productCode;
        else if (edit == DOCUMENTED_2000)
            id.modelCode = L::documentedIdentity().modelCode; // The EDS default admits nothing alone.
        else if (edit == OTHER_BOOT)
            ++id.target.nodeGeneration;
        else
            id = L::IdentityObservation();
        const unsigned sent = r.wire.sentCount;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status s =
            L::prepareModeSelect(action, r.reservation, r.state, id, 3, r.operation, r.now, r.deadline, r.policy());
        CHECK(!s && s.code == M::Err::UNSUPPORTED && s.detail == int32_t(M::ActionError::TARGET));
        empty.unchanged(action);
        CHECK(r.wire.sentCount == sent);
        // An already selected mode is still answered, with the comparison recorded where it could be made.
        Rig same;
        acquire(same, RELEASED_STATUS, RELEASED_MONITOR, 3);
        same.begin();
        L::IdentityObservation sameId = id;
        if (edit != NONE_READ)
            sameId.target = same.reservation.target;
        if (edit == OTHER_BOOT)
            ++sameId.target.nodeGeneration;
        CHECK(L::prepareModeSelect(action, same.reservation, same.state, sameId, 3, same.operation, same.now,
                                   same.deadline, same.policy()));
        CHECK(action.modeSelect.alreadySelected && action.state == M::ActionState::SUCCEEDED &&
              action.execution == M::ActionExecution::NOT_TRANSMITTED);
        if (edit == OTHER_BOOT || edit == NONE_READ)
            CHECK(action.modeSelect.identity.outcome == L::IdentityOutcome::NONE);
        else
            CHECK(action.modeSelect.identity.outcome == L::IdentityOutcome::UNCONFIRMED &&
                  !action.modeSelect.identity.expectedMatch);
    }
    // A state refusal comes first: an enabled report with a foreign identity names Q11.
    {
        Rig r;
        acquire(r, MODE_ONE_STATUS, POWER_UP_MONITOR, 1);
        r.begin();
        L::IdentityObservation id = identityOf(r);
        ++id.modelCode;
        L::ActionContext action;
        const M::Status s =
            L::prepareModeSelect(action, r.reservation, r.state, id, 3, r.operation, r.now, r.deadline, r.policy());
        CHECK(!s && s.detail == int32_t(M::ActionError::STATE) && std::strstr(s.msg, "Q11"));
    }
    // Each firmware field that differs admits the write and flags it; the measured identity does not.
    for (int field = 0; field <= 4; ++field) {
        Rig r;
        acquire(r, RELEASED_STATUS, RELEASED_MONITOR, 1);
        r.begin();
        L::IdentityObservation id = identityOf(r);
        if (field == 1)
            ++id.hardwareVersion;
        else if (field == 2)
            ++id.softwareVersion;
        else if (field == 3)
            ++id.revision;
        else if (field == 4)
            ++id.firmwareCode;
        L::ActionContext action;
        CHECK(L::prepareModeSelect(action, r.reservation, r.state, id, 3, r.operation, r.now, r.deadline,
                                   r.policy()));
        const L::IdentityCheck& check = action.modeSelect.identity;
        CHECK(check.outcome == L::IdentityOutcome::CONFIRMED && check.basis == L::IdentityBasis::MEASURED &&
              check.expectedMatch && check.firmwareUnqualified == (field != 0) &&
              check.observed.softwareVersion == id.softwareVersion &&
              check.expected.softwareVersion == L::measuredIdentity().softwareVersion);
        r.wire.action = &action;
        select(r, action, 3);
    }
}
} // namespace
int main() {
    admittedStarts();
    refusedBeforeTransmission();
    undocumentedCodes();
    budgetsAndOverlap();
    settlesOnModeReport();
    cycleOutsideScope();
    deadlineWithoutModeReport();
    writeFailureAtEveryStage();
    shapeIsChecked();
    alreadySelected();
    identityRule();
    std::printf("mode select: %u checks passed\n", checks);
}
