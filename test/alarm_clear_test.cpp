// SPDX-License-Identifier: MIT
// Packet 17: the explicit alarm clear through the real owner and SDO client, with synthetic
// traffic only. Host evidence: no drive has run any of this.
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
// Rig::addSecondNode binds node 2 on slot 1; that needs a capacity set with two channels.
constexpr bool PAIRED_CHANNELS = C::SdoClient::CHANNELS >= 2;
const uint16_t CLEAR = 0x2301;
const uint16_t CONTROL = 0x6040;
const uint16_t ALARM = L::MONITOR_ALARM;
const L::AlarmClearMethod METHODS[] = {L::AlarmClearMethod::OBJECT_2301,
                                       L::AlarmClearMethod::CONTROLWORD_BIT7};
// The one object a method writes.
uint16_t written(L::AlarmClearMethod method) {
    return method == L::AlarmClearMethod::CONTROLWORD_BIT7 ? CONTROL : CLEAR;
}
// 6041 as node 1 reported it on 2026-11-13: C018 with the FF05 alarm (bit 3 set, low bits 000), 8010 after
// the 2301 clear (trial 5323), and 8050, the 0x50 pattern of every measured release.
const uint16_t ALARMED_STATUS = 0xc018, CLEARED_STATUS = 0x8010, RELEASED_STATUS = 0x8050;

M::Status attempt(Rig& r, L::ActionContext& action) {
    r.begin();
    return L::prepareClearAlarm(action, r.reservation, r.state, r.operation, r.now, r.deadline,
                                r.policy(), r.clearMethod);
}
// Poll a persisting alarm for `cycles` complete cycles, then let the deadline end the attempt.
void persistUntilDeadline(Rig& r, L::ActionContext& action, unsigned cycles) {
    for (unsigned cycle = 0; cycle < cycles; ++cycle) {
        r.cycle(action, ALARM, 0);
        CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING);
        CHECK(!r.next(action).ready);
        r.turn(action.eligibleUs, O::ServiceBudget());
    }
    r.turn(r.deadline, O::ServiceBudget());
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.state == M::ActionState::FAILED && action.outcome == M::ActionOutcome::DEADLINE);
}

// Only FF05 with a reported alarm is admitted, on either 86C model: here mode 1 with running clear,
// mode 3 in modeThreeAdmission. The code narrows the attempt; it never identifies the alarm.
void admissionScope() {
    for (unsigned model = 0; model < 2; ++model) {
        Rig r;
        if (model)
            r.reservation.model = L::SettingsModel::OL86_C;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const L::AlarmIncident& incident = action.incident;
        CHECK(incident.present && incident.codeLink == L::AlarmCodeLink::UNRESOLVED);
        CHECK(incident.errorCode == 0xff05 && incident.errorRegister == 1 && incident.monitor == ALARM);
        CHECK(!action.baseline.present && !action.valueOneMayRemain && !action.coordinatesInvalidated);
        CHECK(action.actionKind == M::ActionKind::CLEAR_ALARM && action.phase == L::ActionPhase::COMMAND);
    }
    // FF01 is the only other named code, FF02..FF04 are never derived from LED counts, and zero or
    // an unknown code is not a narrower case.
    const uint16_t codes[] = {0, 0x0005, 0x05ff, 0xff00, 0xff01, 0xff02, 0xff03, 0xff04, 0xff06, 0xffff};
    for (uint16_t code : codes) {
        Rig r;
        r.acquire(ALARM, 1, code);
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status status = attempt(r, action);
        CHECK(!status && status.detail == int32_t(M::ActionError::STATE));
        empty.unchanged(action);
    }
    struct Case {
        uint16_t monitor;
        uint8_t mode, mask;
    };
    const uint8_t all = 0x1f, noErrors = 0x1e, noHistory = 0x1d;
    const Case cases[] = {{0, 1, all},
                          {uint16_t(ALARM | L::MONITOR_RUNNING), 1, all},
                          {ALARM, 0, all},
                          {ALARM, 3, all}, // Mode 3 without the release.
                          {ALARM, 6, all},
                          {ALARM, 1, noErrors},
                          {ALARM, 1, noHistory}};
    for (const Case& item : cases) {
        Rig r;
        r.acquire(item.monitor, item.mode, 0xff05, item.mask);
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        const M::Status status = attempt(r, action);
        CHECK(!status && status.detail == int32_t(M::ActionError::STATE));
        empty.unchanged(action);
    }
    // A reservation for another configuration epoch, or an unbound model, is not this read's.
    for (unsigned which = 0; which < 2; ++which) {
        Rig r;
        r.acquire(ALARM);
        r.begin();
        L::AxisReservation stale = r.reservation;
        if (which)
            stale.model = L::SettingsModel::UNKNOWN;
        else
            ++stale.target.channel.binding.configurationGeneration;
        L::ActionContext action;
        const Saved<L::ActionContext> empty(action);
        CHECK(!L::prepareClearAlarm(action, stale, r.state, r.operation, r.now, r.deadline, r.policy()));
        empty.unchanged(action);
    }
}

// Mode 3 is project policy since the bench finding of 2026-11-13: node 1 raised FF05 in a mode 3
// run and then read 2100=001D (in place, running, alarm, released), 6041=C018 and 606C 0, which the
// mode 1 rule refused. Admitted released, with running clear, or running set as that latched
// release with 606C zero in the same read's FEEDBACK block. The writes, the cycles and the final
// read are as in mode 1; completion needs 6041 low bits 000 and its alarm bits clear, the release
// either way, as trial 5323 reported after the 2301 clear (2100=0001, 6041=8010).
const uint16_t IN_PLACE = L::MONITOR_POSITION_COMPLETE;
const uint16_t RELEASED = L::MONITOR_RELEASED;
const uint16_t LATCH = L::MONITOR_RUNNING;
const uint16_t RELEASED_ALARM = uint16_t(IN_PLACE | ALARM | RELEASED);
const uint16_t LATCHED_ALARM = uint16_t(IN_PLACE | LATCH | ALARM | RELEASED);
void modeThreeAdmission() {
    CHECK(RELEASED_ALARM == 0x0019 && LATCHED_ALARM == 0x001d);
    struct Admitted {
        uint16_t monitor, after, status;
        uint8_t mask;
    };
    // The latch clears with the alarm, and the release may stay or go.
    const Admitted admitted[] = {{RELEASED_ALARM, uint16_t(IN_PLACE | RELEASED), RELEASED_STATUS, 0x1f},
                                 {RELEASED_ALARM, uint16_t(IN_PLACE | RELEASED), RELEASED_STATUS, 0x0f},
                                 {RELEASED_ALARM, IN_PLACE, CLEARED_STATUS, 0x0f},
                                 {LATCHED_ALARM, uint16_t(IN_PLACE | RELEASED), RELEASED_STATUS, 0x1f},
                                 {LATCHED_ALARM, IN_PLACE, CLEARED_STATUS, 0x1f}}; // Trial 5323.
    for (L::AlarmClearMethod method : METHODS)
        for (const Admitted& item : admitted) {
            Rig r;
            r.clearMethod = method;
            r.errorRegister = 0x81; // As node 1 read it.
            r.statusword = ALARMED_STATUS;
            r.acquire(item.monitor, 3, 0xff05, item.mask);
            L::ActionContext action;
            r.prepare(action, false);
            const L::AlarmIncident& incident = action.incident;
            CHECK(incident.present && incident.codeLink == L::AlarmCodeLink::UNRESOLVED &&
                  incident.modeRaw == 3 && incident.monitor == item.monitor &&
                  incident.errorCode == 0xff05 && incident.errorRegister == 0x81 &&
                  incident.statusword == ALARMED_STATUS);
            CHECK(action.actionKind == M::ActionKind::CLEAR_ALARM && action.clearMethod == method &&
                  action.phase == L::ActionPhase::COMMAND && !action.valueOneMayRemain &&
                  !action.coordinatesInvalidated);
            const Saved<L::AlarmIncident> kept(action.incident);
            const unsigned from = r.wire.sentCount;
            // 2B 01 23 00 01 00 00 00, then 2B 01 23 00 00 00 00 00; or 2B 40 60 00 80 00 00 00, then
            // 2B 40 60 00 00 00 00 00 (Rig::command compares the bytes).
            r.commands(action, false);
            r.statusword = item.status;
            r.cycle(action, item.after, 0, 0xff05, 3);
            CHECK(action.state == M::ActionState::SUCCEEDED &&
                  action.outcome == M::ActionOutcome::OBSERVED &&
                  action.completion == M::ActionCompletion::OBSERVED && !action.valueOneMayRemain);
            CHECK(action.monitorObservation.monitor == item.after &&
                  action.statusObservation.statusword == item.status &&
                  action.statusObservation.modeRaw == 3 && action.observations == 1);
            CHECK(action.extraObservation.block == L::StateBlock::ERRORS &&
                  action.extraObservation.errorCode == 0xff05);
            // Only the method's two writes: no 6060, target or any other download, and for 2301 no 6040.
            // The cycles read MONITORS and STATUS, then ERRORS once; never FEEDBACK or the history.
            r.onlyActionTraffic(from, written(method), 2);
            CHECK(r.uploadsOf(from, 0x6064) == 0 && r.uploadsOf(from, 0x606c) == 0 &&
                  r.uploadsOf(from, 0x1003) == 0 && r.uploadsOf(from, 0x603f) == 1);
            kept.unchanged(action.incident);
        }
    struct Refused {
        uint16_t monitor;
        uint8_t mode, mask;
        int32_t velocity;
    };
    const Refused refused[] = {
        {LATCHED_ALARM, 3, 0x1f, 1},  // 606C nonzero beside the latch: not the latched release.
        {LATCHED_ALARM, 3, 0x1f, -1},
        {LATCHED_ALARM, 3, 0x0f, 0},  // No FEEDBACK block: the latch cannot be told from a run.
        {uint16_t(IN_PLACE | ALARM), 3, 0x1f, 0},         // Not released.
        {uint16_t(IN_PLACE | LATCH | ALARM), 3, 0x1f, 0}, // Running and not released.
        {uint16_t(IN_PLACE | RELEASED), 3, 0x1f, 0},      // No alarm.
        {RELEASED_ALARM, 3, 0x1e, 0},                     // No ERRORS.
        {RELEASED_ALARM, 3, 0x1d, 0},                     // No ERROR_HISTORY.
        {LATCHED_ALARM, 1, 0x1f, 0},  // Mode 1 admits no running report, latched or not.
        {RELEASED_ALARM, 0, 0x1f, 0},
        {LATCHED_ALARM, 0, 0x1f, 0},
        {RELEASED_ALARM, 6, 0x1f, 0},
        {LATCHED_ALARM, 6, 0x1f, 0},
        {RELEASED_ALARM, 2, 0x1f, 0},
        {RELEASED_ALARM, 0x83, 0x1f, 0}, // The complete byte decides, not its low bits.
    };
    // Either method is refused alike: the method changes the writes, never the admission.
    for (L::AlarmClearMethod method : METHODS)
        for (const Refused& item : refused) {
            Rig r;
            r.clearMethod = method;
            r.feedbackVelocity = item.velocity;
            r.acquire(item.monitor, item.mode, 0xff05, item.mask);
            const unsigned from = r.wire.sentCount;
            L::ActionContext action;
            const Saved<L::ActionContext> empty(action);
            const M::Status status = attempt(r, action);
            CHECK(!status && status.detail == int32_t(M::ActionError::STATE));
            empty.unchanged(action);
            CHECK(r.wire.sentCount == from);
        }
    // Any other code is refused in mode 3 too.
    const uint16_t codes[] = {0, 0xff01, 0xff04};
    for (L::AlarmClearMethod method : METHODS)
        for (uint16_t code : codes) {
            Rig r;
            r.clearMethod = method;
            r.acquire(LATCHED_ALARM, 3, code);
            L::ActionContext action;
            const Saved<L::ActionContext> empty(action);
            CHECK(!attempt(r, action));
            empty.unchanged(action);
        }
}

// Completion needs the admission's mode. After a mode 3 admission every cycle must read mode 3 and 6041
// low bits 000; while the alarm is reported, released too, and running only as the latch the admission
// read; once it is not, running clear, the release either way. Anything else ends the attempt
// OBSERVATION_SCOPE, even with the alarm reported clear: no final ERRORS read, no further write. A mode 1
// admission is held to mode 1 in the same way, and 6041 decides nothing there. Both methods alike.
void completionNeedsTheAdmissionMode() {
    struct Case {
        uint16_t admitted;
        uint8_t mode;
        uint16_t report;
        uint8_t reportMode;
        uint16_t status;
    };
    const Case outside[] = {
        {RELEASED_ALARM, 3, uint16_t(IN_PLACE | RELEASED), 1, RELEASED_STATUS},        // Changed to mode 1.
        {LATCHED_ALARM, 3, uint16_t(IN_PLACE | LATCH | RELEASED), 1, RELEASED_STATUS}, // The same after a latch.
        {LATCHED_ALARM, 3, IN_PLACE, 1, CLEARED_STATUS}, // Trial 5323's report, but in mode 1.
        {RELEASED_ALARM, 3, uint16_t(IN_PLACE | RELEASED), 0, RELEASED_STATUS},
        {RELEASED_ALARM, 3, uint16_t(IN_PLACE | RELEASED), 6, RELEASED_STATUS},
        // Running reported with the alarm clear, after an admission with or without the latch.
        {RELEASED_ALARM, 3, uint16_t(IN_PLACE | LATCH | RELEASED), 3, RELEASED_STATUS},
        {LATCHED_ALARM, 3, uint16_t(IN_PLACE | LATCH | RELEASED), 3, RELEASED_STATUS},
        {LATCHED_ALARM, 3, uint16_t(IN_PLACE | LATCH), 3, CLEARED_STATUS},
        // Running beside the alarm, where the admission read no latch.
        {RELEASED_ALARM, 3, uint16_t(IN_PLACE | LATCH | ALARM | RELEASED), 3, ALARMED_STATUS},
        // The release gone while the alarm is still reported, beside the latch or not.
        {RELEASED_ALARM, 3, uint16_t(IN_PLACE | ALARM), 3, ALARMED_STATUS},
        {LATCHED_ALARM, 3, uint16_t(IN_PLACE | LATCH | ALARM), 3, ALARMED_STATUS},
        // Any switched-on 6041 pattern, with the alarm reported clear or not: word 6, 7 and 15 reports
        // (8031, 8033, 8037), a quick stop's 8027, and 8437, energized at standstill.
        {RELEASED_ALARM, 3, uint16_t(IN_PLACE | RELEASED), 3, 0x8031},
        {LATCHED_ALARM, 3, IN_PLACE, 3, 0x8033},
        {LATCHED_ALARM, 3, IN_PLACE, 3, 0x8037},
        {RELEASED_ALARM, 3, IN_PLACE, 3, 0x8027},
        {RELEASED_ALARM, 3, IN_PLACE, 3, 0x8437},
        {RELEASED_ALARM, 3, RELEASED_ALARM, 3, 0xc019},
        {LATCHED_ALARM, 3, LATCHED_ALARM, 3, 0xc01f},
        {ALARM, 1, 0, 3, CLEARED_STATUS}, // A mode 1 admission in mode 3.
    };
    for (L::AlarmClearMethod method : METHODS)
        for (const Case& item : outside) {
            Rig r;
            r.clearMethod = method;
            r.statusword = ALARMED_STATUS;
            r.acquire(item.admitted, item.mode);
            L::ActionContext action;
            r.prepare(action, false);
            const unsigned from = r.wire.sentCount;
            r.commands(action, false);
            r.statusword = item.status;
            r.cycle(action, item.report, 0, 0xff05, item.reportMode);
            CHECK(action.state == M::ActionState::FAILED &&
                  action.outcome == M::ActionOutcome::OBSERVATION_SCOPE &&
                  action.completion == M::ActionCompletion::NOT_OBSERVED && !action.finalErrorRead);
            CHECK(action.execution == M::ActionExecution::ACKNOWLEDGED && action.uncertain &&
                  !action.valueOneMayRemain);
            CHECK(action.monitorObservation.monitor == item.report &&
                  action.statusObservation.statusword == item.status &&
                  action.statusObservation.modeRaw == item.reportMode);
            CHECK(!r.next(action).ready);
            r.onlyActionTraffic(from, written(method), 2);
            CHECK(r.uploadsOf(from, 0x603f) == 0 && r.uploadsOf(from, 0x1001) == 0);
        }
    // In scope but not complete, so the attempt repolls: the alarm still reported beside the latch or
    // the release, or 2100 reporting it clear while 6041 bit 3 (ZH86 p12) or bit 7 (EN86 p12, OP86 p10)
    // still reads set. A later report of trial 5323's shape completes it.
    struct Pending {
        uint16_t admitted, report, status;
    };
    const Pending pending[] = {{LATCHED_ALARM, LATCHED_ALARM, ALARMED_STATUS},
                               {RELEASED_ALARM, RELEASED_ALARM, ALARMED_STATUS},
                               {LATCHED_ALARM, IN_PLACE, 0x8018},
                               {LATCHED_ALARM, IN_PLACE, 0x8090},
                               {RELEASED_ALARM, uint16_t(IN_PLACE | RELEASED), 0x8058},
                               {RELEASED_ALARM, uint16_t(IN_PLACE | RELEASED), 0x80d0}};
    for (L::AlarmClearMethod method : METHODS)
        for (const Pending& item : pending) {
            Rig r;
            r.clearMethod = method;
            r.statusword = ALARMED_STATUS;
            r.acquire(item.admitted, 3);
            L::ActionContext action;
            r.prepare(action, false);
            const unsigned from = r.wire.sentCount;
            r.commands(action, false);
            r.statusword = item.status;
            r.cycle(action, item.report, 0, 0xff05, 3);
            CHECK(action.state == M::ActionState::ACTIVE && action.phase == L::ActionPhase::WAITING &&
                  !action.finalErrorRead);
            CHECK(!r.next(action).ready);
            r.turn(action.eligibleUs, O::ServiceBudget());
            r.statusword = CLEARED_STATUS;
            r.cycle(action, IN_PLACE, 0, 0, 3);
            CHECK(action.state == M::ActionState::SUCCEEDED && action.observations == 2 &&
                  action.extraObservation.errorCode == 0);
            r.onlyActionTraffic(from, written(method), 2);
        }
    // A mode 1 attempt judges 2100 alone: the rig's 6041 of FFFF, every bit set, still completes it.
    for (L::AlarmClearMethod method : METHODS) {
        Rig r;
        r.clearMethod = method;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        r.commands(action, false);
        r.cycle(action);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.statusObservation.statusword == 0xffff);
    }
}

// Trial 5323, node 1, 2026-11-13, image 6dee27b: the 2301 clear admitted in mode 3 from 2100=001D,
// 6041=C018, 6061=3, 606C 0, 1001=81, 603F=FF05 and a 1003 count of 0. The first cycle after the 0's
// acknowledgement read 2100=0001, 2101=0000, 2102=0000, 6041=8010 and 6061=3: alarm, running and the
// release clear, low bits 000. The rule of that image, released in every cycle, ended it
// OBSERVATION_SCOPE (failure=10 why=clear); this rule completes it, then reads ERRORS once. The same
// report completes the controlword method, which has not run on a specimen.
void trialFiveThreeTwoThree() {
    for (L::AlarmClearMethod method : METHODS) {
        Rig r;
        r.clearMethod = method;
        r.errorRegister = 0x81;
        r.statusword = 0xc018;
        r.acquire(0x001d, 3, 0xff05);
        L::ActionContext action;
        r.prepare(action, false);
        CHECK(action.incident.monitor == 0x001d && action.incident.statusword == 0xc018 &&
              action.incident.modeRaw == 3 && action.incident.errorRegister == 0x81);
        const unsigned from = r.wire.sentCount;
        r.commands(action, false);
        r.statusword = 0x8010;
        r.cycle(action, 0x0001, 0, 0xff05, 3);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.outcome == M::ActionOutcome::OBSERVED &&
              action.completion == M::ActionCompletion::OBSERVED && action.observations == 1);
        CHECK(action.monitorObservation.monitor == 0x0001 && action.statusObservation.statusword == 0x8010 &&
              action.statusObservation.modeRaw == 3);
        // The final ERRORS read keeps 603F raw: node 1 still read FF05 there, beside 1001.
        CHECK(action.extraObservation.block == L::StateBlock::ERRORS &&
              action.extraObservation.errorCode == 0xff05 && action.extraObservation.errorRegister == 0x81);
        r.onlyActionTraffic(from, written(method), 2);
        CHECK(r.uploadsOf(from, 0x603f) == 1 && r.uploadsOf(from, 0x1001) == 1 && r.uploadsOf(from, 0x1003) == 0);
    }
}

// The controlword method: 6040 U16 0x0080, then 0x0000 only after the 0x0080's acknowledgement, and no
// other download. It shares the 2301 method's admission and incident exactly, and an unknown method is
// refused before any check of the read.
void controlwordMethod() {
    Rig r;
    r.clearMethod = L::AlarmClearMethod::CONTROLWORD_BIT7;
    r.acquire(ALARM);
    L::ActionContext action;
    r.prepare(action, false);
    CHECK(action.clearMethod == L::AlarmClearMethod::CONTROLWORD_BIT7 &&
          action.actionKind == M::ActionKind::CLEAR_ALARM);
    const unsigned from = r.wire.sentCount;
    // The first word is yielded again, unchanged, until its own terminal: the 0x0000 never precedes it.
    const L::PreparedAction first = r.next(action);
    CHECK(first.ready && first.transfer.index == CONTROL && first.transfer.subindex == 0 &&
          first.transfer.size == 2 && first.transfer.data[0] == 0x80 && first.transfer.data[1] == 0);
    const O::RequestId id = r.admit(first.transfer);
    r.turn(r.now);
    const L::PreparedAction again = r.next(action);
    CHECK(again.ready && again.transfer.step == first.transfer.step && again.transfer.data[0] == 0x80);
    r.wire.push(reply(first.transfer), r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    CHECK(L::advanceAction(action, r.reservation, r.current, r.result(id), r.now));
    CHECK(action.execution == M::ActionExecution::ACKNOWLEDGED && action.commandCursor == 1 &&
          action.commandHistory[0].transfer.index == CONTROL && action.commandHistory[0].transfer.data[0] == 0x80);
    r.command(action, CONTROL, 0x0000); // 2B 40 60 00 00 00 00 00.
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED && action.commandHistory[1].transfer.data[0] == 0);
    r.onlyActionTraffic(from, CONTROL, 2);
    const uint8_t words[2][8] = {{0x2b, 0x40, 0x60, 0, 0x80, 0, 0, 0}, {0x2b, 0x40, 0x60, 0, 0, 0, 0, 0}};
    unsigned seen = 0;
    for (unsigned i = from; i < r.wire.sentCount; ++i)
        if (isDownload(r.wire.sent[i]))
            CHECK(seen < 2 && !std::memcmp(r.wire.sent[i].data, words[seen++], 8));
    CHECK(seen == 2);
    // Admission parity: each case admits or refuses alike under both methods, with the same detail, and
    // an admitted context differs only in clearMethod.
    struct Case {
        uint16_t monitor;
        uint8_t mode, mask;
        uint16_t code;
        int32_t velocity;
    };
    const Case cases[] = {{ALARM, 1, 0x1f, 0xff05, 0},          {ALARM, 1, 0x0f, 0xff05, 0},
                          {RELEASED_ALARM, 3, 0x0f, 0xff05, 0}, {LATCHED_ALARM, 3, 0x1f, 0xff05, 0},
                          {LATCHED_ALARM, 3, 0x1f, 0xff05, 5},  {LATCHED_ALARM, 3, 0x0f, 0xff05, 0},
                          {uint16_t(IN_PLACE | ALARM), 3, 0x1f, 0xff05, 0}, // Mode 3 not released.
                          {uint16_t(ALARM | LATCH), 1, 0x1f, 0xff05, 0},    {0, 1, 0x1f, 0xff05, 0},
                          {ALARM, 1, 0x1f, 0xff01, 0},          {ALARM, 6, 0x1f, 0xff05, 0},
                          {ALARM, 1, 0x1e, 0xff05, 0}};
    for (const Case& item : cases) {
        L::ActionContext results[2];
        M::Status statuses[2];
        for (unsigned m = 0; m < 2; ++m) {
            Rig s;
            s.clearMethod = METHODS[m];
            s.feedbackVelocity = item.velocity;
            s.acquire(item.monitor, item.mode, item.code, item.mask);
            statuses[m] = attempt(s, results[m]);
            CHECK(results[m].state == (statuses[m] ? M::ActionState::ACTIVE : M::ActionState::EMPTY));
        }
        CHECK(bool(statuses[0]) == bool(statuses[1]) && statuses[0].detail == statuses[1].detail);
        if (!statuses[0])
            continue;
        const L::AlarmIncident &a = results[0].incident, &b = results[1].incident;
        CHECK(a.present && b.present && a.codeLink == b.codeLink && a.readOperation == b.readOperation &&
              a.earliestUs == b.earliestUs && a.latestUs == b.latestUs && a.errorRegister == b.errorRegister &&
              a.errorCode == b.errorCode && a.errorCount == b.errorCount &&
              !std::memcmp(a.errorHistory, b.errorHistory, sizeof a.errorHistory) && a.monitor == b.monitor &&
              a.logicalInputs == b.logicalInputs && a.logicalOutputs == b.logicalOutputs &&
              a.statusword == b.statusword && a.modeRaw == b.modeRaw);
        CHECK(results[0].clearMethod == METHODS[0] && results[1].clearMethod == METHODS[1] &&
              results[0].phase == results[1].phase && results[0].operation == results[1].operation &&
              results[0].deadlineUs == results[1].deadlineUs && results[0].step == results[1].step &&
              results[0].commandCursor == 0 && results[1].commandCursor == 0);
    }
    // An unknown method is refused with ARGUMENT, before anything else, and changes nothing.
    Rig u;
    u.acquire(ALARM);
    u.begin();
    L::ActionContext refused;
    const Saved<L::ActionContext> empty(refused);
    const M::Status status = L::prepareClearAlarm(refused, u.reservation, u.state, u.operation, u.now, u.deadline,
                                                  u.policy(), L::AlarmClearMethod(2));
    CHECK(!status && status.detail == int32_t(M::ActionError::ARGUMENT));
    empty.unchanged(refused);
}

// The admission's mode and monitor are part of the checked shape: a context holding an incident
// that prepareClearAlarm could not have admitted is refused by every call, so no edit widens
// completion.
void incidentShapeIsChecked() {
    Rig r;
    r.acquire(LATCHED_ALARM, 3);
    L::ActionContext action;
    r.prepare(action, false);
    const Saved<L::ActionContext> admitted(action);
    L::PreparedAction work;
    CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && work.ready);
    const uint8_t modes[] = {0, 1, 2, 6};
    for (uint8_t mode : modes) {
        action.incident.modeRaw = mode;
        CHECK(!L::nextAction(action, r.reservation, r.current, r.now, work));
    }
    action.incident.modeRaw = 3;
    const uint16_t monitors[] = {uint16_t(LATCHED_ALARM & ~RELEASED),
                                 uint16_t(LATCHED_ALARM & ~ALARM)};
    for (uint16_t monitor : monitors) {
        action.incident.monitor = monitor;
        CHECK(!L::nextAction(action, r.reservation, r.current, r.now, work));
    }
    action.incident.monitor = LATCHED_ALARM;
    action.incident.present = false;
    CHECK(!L::nextAction(action, r.reservation, r.current, r.now, work));
    action.incident.present = true;
    admitted.unchanged(action);
    CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && work.ready &&
          work.transfer.index == CLEAR && work.transfer.data[0] == 1);
}

// A retained 603F=FF05 beside a later alarm admits exactly as a fresh one would. The application
// can see that the code did not change since a report without the alarm, so the code is stale
// evidence; the action stays UNRESOLVED either way and claims nothing about the later cause.
void staleCodeWithLaterIncident() {
    Rig r;
    r.acquire(ALARM);
    L::ActionContext first;
    r.prepare(first, false);
    r.commands(first, false);
    r.cycle(first); // Alarm clear, then the one ERRORS read: 603F still FF05.
    CHECK(first.state == M::ActionState::SUCCEEDED && first.extraObservation.errorCode == 0xff05);
    const Saved<L::ActionContext> firstDone(first);

    const unsigned from = r.wire.sentCount;
    r.acquire(ALARM); // A later incident with an unknown cause; 603F reads the same code.
    L::ActionContext second;
    r.prepare(second, false);
    CHECK(second.incident.codeLink == L::AlarmCodeLink::UNRESOLVED &&
          second.incident.errorCode == first.extraObservation.errorCode &&
          second.incident.readOperation != first.incident.readOperation);
    const Saved<L::AlarmIncident> incident(second.incident);
    const unsigned admitted = r.wire.sentCount;
    r.commands(second, false);
    // The later alarm does not clear. Only MONITORS and STATUS are polled, no write repeats.
    persistUntilDeadline(r, second, 3);
    CHECK(second.execution == M::ActionExecution::ACKNOWLEDGED && second.uncertain &&
          second.completion == M::ActionCompletion::NOT_OBSERVED && !second.valueOneMayRemain);
    CHECK(second.observations == 3 && !second.finalErrorRead);
    r.onlyActionTraffic(from, CLEAR, 2);
    CHECK(r.uploadsOf(admitted, 0x603f) == 0 && r.uploadsOf(admitted, 0x1001) == 0 &&
          r.uploadsOf(admitted, 0x1003) == 0);
    incident.unchanged(second.incident);
    firstDone.unchanged(first);
}

// An observed alarm-clear report succeeds whatever ERRORS then reads, and keeps it raw: the
// value may stay latched, return to zero, or name another code. ERRORS is read once, 1003 never.
void latchedErrorValue() {
    const uint16_t finals[] = {0xff05, 0, 0xff01};
    for (uint16_t code : finals) {
        Rig r;
        r.errorRegister = 0x81;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const Saved<L::AlarmIncident> incident(action.incident);
        const unsigned admitted = r.wire.sentCount;
        r.commands(action, false);
        r.cycle(action, 0, 0, code);
        CHECK(action.state == M::ActionState::SUCCEEDED && action.outcome == M::ActionOutcome::OBSERVED &&
              action.completion == M::ActionCompletion::OBSERVED && !action.valueOneMayRemain);
        CHECK(action.extraObservation.block == L::StateBlock::ERRORS &&
              action.extraObservation.errorCode == code &&
              action.extraObservation.errorRegister == (code ? 0x81 : 0));
        CHECK(r.uploadsOf(admitted, 0x603f) == 1 && r.uploadsOf(admitted, 0x1001) == 1 &&
              r.uploadsOf(admitted, 0x1003) == 0);
        incident.unchanged(action.incident);
        CHECK(action.incident.codeLink == L::AlarmCodeLink::UNRESOLVED);
        r.onlyActionTraffic(admitted, CLEAR, 2);
    }
}

// No stop follows a reset word, and the two methods never cross: a 6040 terminal cannot settle the
// 2301 method's work, nor a 2301 terminal the controlword method's, even with every identity copied.
// A context whose method is out of range, or set on another action, is refused by every call.
void methodsDoNotCross() {
    const uint16_t words[] = {0x0080, 0x008f, 0x0000};
    for (uint16_t word : words)
        CHECK(!L::stopPredecessorAllowed(word, L::StopKind::HALT) &&
              !L::stopPredecessorAllowed(word, L::StopKind::QUICK_STOP));
    for (L::AlarmClearMethod method : METHODS) {
        const bool control = method == L::AlarmClearMethod::CONTROLWORD_BIT7;
        Rig r;
        r.clearMethod = method;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const unsigned from = r.wire.sentCount;
        const L::PreparedAction work = r.next(action);
        CHECK(work.ready && work.transfer.index == written(method) &&
              work.transfer.data[0] == (control ? 0x80 : 1));
        C::SdoCompletion forged;
        forged.transfer = work.transfer;
        forged.transfer.index = control ? CLEAR : CONTROL;
        forged.transfer.data[0] = control ? 1 : 0x80;
        forged.outcome = C::SdoOutcome::SUCCESS;
        forged.admittedUs = forged.completedUs = r.now;
        forged.nodeGeneration = r.current.nodeGeneration;
        forged.queued = forged.transmissionPossible = forged.transmissionConfirmed = true;
        forged.protocolAcknowledged = forged.effectPossible = true;
        const Saved<L::ActionContext> before(action);
        const M::Status status = L::advanceAction(action, r.reservation, r.current, forged, r.now);
        CHECK(!status && status.detail == int32_t(M::ActionError::CORRELATION));
        before.unchanged(action);
        // An out-of-range method is refused by every call.
        L::PreparedAction blank;
        action.clearMethod = L::AlarmClearMethod(2);
        CHECK(!L::nextAction(action, r.reservation, r.current, r.now, blank));
        action.clearMethod = method;
        before.unchanged(action);
        r.commands(action, false);
        r.cycle(action);
        CHECK(action.state == M::ActionState::SUCCEEDED);
        r.onlyActionTraffic(from, written(method), 2); // No byte of the other object left the owner.
    }
    // Only an alarm clear carries the controlword method.
    Rig r;
    r.acquire(0);
    L::ActionContext counter;
    r.prepare(counter, true);
    L::PreparedAction work;
    CHECK(L::nextAction(counter, r.reservation, r.current, r.now, work) && work.ready);
    counter.clearMethod = L::AlarmClearMethod::CONTROLWORD_BIT7;
    CHECK(!L::nextAction(counter, r.reservation, r.current, r.now, work));
}

// Each write fails at each boundary, under both methods. The prefix stays, nothing replays, and the
// value-1 consequence is set exactly when the first value (2301 1, or 6040 0x0080) may have landed and
// the 0 has no positive ACK.
void prefixFailureAtEveryStage() {
    for (L::AlarmClearMethod method : METHODS)
        for (unsigned slot = 0; slot < 2; ++slot)
            for (Stage stage : STAGES) {
                const uint16_t index = written(method);
                const uint8_t first = method == L::AlarmClearMethod::CONTROLWORD_BIT7 ? 0x80 : 1;
                Rig r;
                r.clearMethod = method;
                r.acquire(ALARM);
                L::ActionContext action;
                r.prepare(action, false);
                const Saved<L::AlarmIncident> incident(action.incident);
                const unsigned from = r.wire.sentCount;
                if (slot)
                    r.command(action, index, first);
                const Saved<C::SdoCompletion> prefix(action.commandHistory[0]);
                const Rig::Failure f = r.failCommand(action, stage);
                CHECK(action.state == M::ActionState::FAILED &&
                      action.completion == M::ActionCompletion::NOT_OBSERVED);
                const bool acked = acknowledged(stage);
                CHECK(acked == (f.admitted && f.terminal.outcome == C::SdoOutcome::SUCCESS));
                CHECK(action.outcome == outcomeAt(stage));
                CHECK(action.commandEvidenceMask ==
                      uint8_t((1u << (slot + (f.admitted ? 1 : 0))) - 1));
                if (f.admitted)
                    CHECK(action.commandHistory[slot].outcome == f.terminal.outcome);
                if (slot) {
                    prefix.unchanged(action.commandHistory[0]);
                    CHECK(action.commandHistory[0].protocolAcknowledged &&
                          action.execution == M::ActionExecution::ACKNOWLEDGED);
                } else {
                    CHECK(action.execution == (!sent(stage) ? M::ActionExecution::NOT_TRANSMITTED
                                               : stage == Stage::ABORTED ? M::ActionExecution::REJECTED
                                               : acked ? M::ActionExecution::ACKNOWLEDGED
                                                       : M::ActionExecution::UNKNOWN));
                }
                const bool oneLanded = slot || sent(stage);
                CHECK(action.valueOneMayRemain == (oneLanded && !(slot && acked)));
                CHECK(action.uncertain == oneLanded);
                CHECK(r.services.unresolved(0) == ambiguous(stage));
                CHECK(r.wire.sentCount == f.sent + (sent(stage) ? 1u : 0u));
                r.onlyActionTraffic(from, index, slot + (sent(stage) ? 1u : 0u));
                incident.unchanged(action.incident);
                CHECK(r.axis.originKnown && r.reference.nativeKnown); // An alarm clear moves no epoch.
                if (f.admitted)
                    r.lateReplyChangesNothing(action, f);
                // The first value was never sent a second time, and the 0 never without its ACK.
                unsigned ones = 0, zeros = 0;
                for (unsigned i = from; i < r.wire.sentCount; ++i) {
                    ones += isDownload(r.wire.sent[i]) && r.wire.sent[i].data[4] == first;
                    zeros += isDownload(r.wire.sent[i]) && r.wire.sent[i].data[4] == 0;
                }
                CHECK(ones == (oneLanded ? 1u : 0u));
                CHECK(zeros == (slot && sent(stage) ? 1u : 0u));
            }
}

// The final 0 goes out and its ACK comes late: the route stays unresolved, the late ACK settles
// nothing, the value-1 consequence stays, and no new operation can start on that axis.
void delayedFinalZeroAck() {
    Rig r;
    r.acquire(ALARM);
    L::ActionContext action;
    r.prepare(action, false);
    r.command(action, CLEAR, 1);
    const Rig::Failure f = r.failCommand(action, Stage::TX_REPORTED);
    CHECK(f.terminal.outcome == C::SdoOutcome::TIMEOUT && f.terminal.effectUncertain &&
          f.terminal.transmissionConfirmed);
    CHECK(action.valueOneMayRemain && action.execution == M::ActionExecution::ACKNOWLEDGED &&
          action.outcome == M::ActionOutcome::SERVICE_TIMEOUT);
    r.lateReplyChangesNothing(action, f);
    CHECK(r.services.unresolved(0));
    CHECK(r.owner.release(f.id));
    CHECK(!r.owner.beginOperation(r.sequence, r.operation + 1, r.deadline, r.now));
    CHECK(r.services.unresolved(0) && action.valueOneMayRemain);
}

// A boot-up while the final 0 is in flight: the application ends the attempt INVALIDATED. The
// 0 has no ACK, so the value-1 consequence stays; the incident and the 1's ACK stay too.
// The same with the 1 in flight: its noted possible transmission alone sets the consequence,
// though no terminal for it is ever consumed.
void rebootDuringClear() {
    for (unsigned zero = 0; zero < 2; ++zero) {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        if (zero)
            r.command(action, CLEAR, 1);
        const Saved<C::SdoCompletion> one(action.commandHistory[0]);
        const Saved<L::AlarmIncident> incident(action.incident);
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        CHECK(action.transmissionNoted && action.effectsPossible);
        M::Frame boot = frame(0x701, 1);
        boot.data[0] = 0;
        r.wire.push(boot, r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        const C::SdoCompletion terminal = r.result(id);
        CHECK(terminal.outcome == C::SdoOutcome::REBOOT);
        // The lifecycle callback moved the node epoch, so this work no longer correlates.
        CHECK(!L::advanceAction(action, r.reservation, r.current, terminal, r.now));
        CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::INVALIDATED, r.now));
        CHECK(action.outcome == M::ActionOutcome::INVALIDATED && action.valueOneMayRemain &&
              action.commandEvidenceMask == zero);
        CHECK(action.execution ==
              (zero ? M::ActionExecution::ACKNOWLEDGED : M::ActionExecution::UNKNOWN));
        one.unchanged(action.commandHistory[0]);
        incident.unchanged(action.incident);
    }
}

// A new EMCY on this node during the clear: the application withdraws first, and the terminal
// in flight is kept as evidence. The owner keeps only its newest EMCY, so the application's
// own copy of the original is what survives. An EMCY 0000 is retained like any other.
void emcyDuringClear() {
    for (unsigned when = 0; when < 3; ++when) {
        Rig r;
        r.wire.push(emcyFrame(1, 0xff05, 0xa5), r.now + 1);
        r.turn(r.now + 1);
        const C::NetworkEvent original = r.services.node(0)->emcy;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        r.command(action, CLEAR, 1);
        const unsigned settled = when == 0 ? 0 : 2; // Fields of the cycle that settle first.
        if (when)
            r.command(action, CLEAR, 0);
        for (unsigned i = 0; i < settled; ++i) {
            const L::PreparedAction work = r.next(action);
            r.field(action, r.raw(work.transfer, 0, 1, 0xff05, 0));
        }
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        const uint16_t code = when == 2 ? 0x0000 : 0xff05;
        r.wire.push(emcyFrame(1, code, 0x5a), r.now + 1);
        const bool download = work.transfer.direction == C::SdoDirection::DOWNLOAD;
        r.wire.push(reply(work.transfer, download ? 0 : r.raw(work.transfer, 0, 1, 0xff05, 0)), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        CHECK(r.services.node(0)->emcyCount == 2 && r.services.node(0)->emcy.errorCode == code);
        const C::SdoCompletion terminal = r.result(id);
        if (when == 2) {
            // Application policy here: EMCY 0000 is not a new fault. The attempt goes on and
            // still needs its own observations; the EMCY is not recovery evidence.
            CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
            r.cycle(action);
            CHECK(action.state == M::ActionState::SUCCEEDED);
        } else {
            CHECK(L::noteActionWithdrawal(action, r.reservation, r.current,
                                          M::ActionOutcome::DRIVE_FAULT, r.now));
            CHECK(L::advanceAction(action, r.reservation, r.current, terminal, r.now));
            CHECK(action.state == M::ActionState::FAILED &&
                  action.outcome == M::ActionOutcome::DRIVE_FAULT &&
                  action.execution == M::ActionExecution::ACKNOWLEDGED);
            // A clear withdrawn with its 0 in flight: that 0's ACK still counts.
            CHECK(action.valueOneMayRemain == false);
            if (when == 0)
                CHECK(action.commandHistory[1].outcome == C::SdoOutcome::SUCCESS);
            else
                CHECK(action.observationEvidencePresent &&
                      action.observationEvidence.transfer.index == terminal.transfer.index &&
                      action.observationEvidence.transfer.step == terminal.transfer.step &&
                      action.observationEvidence.completedUs == terminal.completedUs);
            CHECK(!r.next(action).ready);
        }
        CHECK(original.errorCode == 0xff05 && original.manufacturer[4] == 0xa5);
        CHECK(r.services.node(0)->emcy.manufacturer[4] == 0x5a);
    }
}

// A second node's EMCY and boot-up change nothing in node 1's clear, and node 2's alarm read
// cannot admit a clear on node 1's reservation.
void twoNodeIsolation() {
    Rig r;
    r.addSecondNode();
    // Node 2's own alarm read, on its own reservation and route.
    CHECK(r.owner.beginOperation(r.sequence2, 1, r.deadline, r.now));
    L::ReadContext other;
    CHECK(L::prepareState(other, r.current2, 1, r.now, r.deadline, options(), 0x0f));
    while (other.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextRead(other, r.now, work) && work.ready);
        const C::SdoCompletion terminal =
            r.exchange(work.transfer, r.raw(work.transfer, ALARM, 1, 0xff05, 0), false, &r.sequence2);
        CHECK(L::advanceRead(other, terminal, r.now));
    }
    CHECK(other.state == M::ReadState::SUCCEEDED);
    r.acquire(ALARM);
    L::ActionContext wrong;
    const Saved<L::ActionContext> empty(wrong);
    CHECK(!L::prepareClearAlarm(wrong, r.reservation, other, r.operation, r.now, r.deadline, r.policy()));
    empty.unchanged(wrong);
    CHECK(!L::prepareClearAlarm(wrong, r.reservation2, r.state, 1, r.now, r.deadline, r.policy()));
    empty.unchanged(wrong);
    L::ActionContext nodeTwo;
    CHECK(L::prepareClearAlarm(nodeTwo, r.reservation2, other, 1, r.now, r.deadline, r.policy()));
    CHECK(nodeTwo.reservation.target.channel.clientToServerId == 0x602);

    L::ActionContext action;
    r.prepare(action, false);
    const unsigned from = r.wire.sentCount;
    r.command(action, CLEAR, 1);
    r.wire.push(emcyFrame(2, 0xff05), r.now + 1);
    M::Frame boot = frame(0x702, 1);
    boot.data[0] = 0;
    r.wire.push(boot, r.now + 1);
    r.turn(r.now + 1, budget(8, 0, 8, 0));
    CHECK(r.services.node(1)->emcyCount == 1 && r.services.node(0)->emcyCount == 0);
    CHECK(r.invalidations == 0 && r.invalidations2 == 1);
    r.command(action, CLEAR, 0);
    r.cycle(action);
    CHECK(action.state == M::ActionState::SUCCEEDED && !action.valueOneMayRemain);
    r.onlyActionTraffic(from, CLEAR, 2);
    CHECK(r.axis.originKnown && r.reference.nativeKnown);
    // Node 2's boot-up withdraws its own prepared clear, and nothing ever went to 0x602.
    L::ReadTarget moved2 = r.current2;
    moved2.nodeGeneration = r.services.node(1)->route.generation;
    CHECK(moved2.nodeGeneration != r.current2.nodeGeneration);
    L::PreparedAction work;
    CHECK(!L::nextAction(nodeTwo, r.reservation2, moved2, r.now, work));
    CHECK(L::advanceAction(nodeTwo, r.reservation2, moved2, M::ActionOutcome::INVALIDATED, r.now));
    CHECK(nodeTwo.outcome == M::ActionOutcome::INVALIDATED && !nodeTwo.valueOneMayRemain &&
          nodeTwo.execution == M::ActionExecution::NOT_TRANSMITTED);
    for (unsigned i = 0; i < r.wire.sentCount; ++i)
        CHECK(!(r.wire.sent[i].id == 0x602 && isDownload(r.wire.sent[i])));
}

// The value-1 flag is part of the checked shape: a context whose flag disagrees with its own
// command history is refused by every call, so a caller cannot clear it or forge it.
void valueOneFlagIsChecked() {
    Rig r;
    r.acquire(ALARM);
    L::ActionContext action;
    r.prepare(action, false);
    L::PreparedAction work;
    action.valueOneMayRemain = true; // On an active context.
    CHECK(!L::nextAction(action, r.reservation, r.current, r.now, work));
    action.valueOneMayRemain = false;
    r.command(action, CLEAR, 1);
    CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
    CHECK(action.valueOneMayRemain);
    CHECK(L::nextAction(action, r.reservation, r.current, r.now, work) && !work.ready);
    action.valueOneMayRemain = false; // Cleared by a caller on a failed context.
    const Saved<L::PreparedAction> blank(work);
    CHECK(!L::nextAction(action, r.reservation, r.current, r.now, work));
    blank.unchanged(work);
    action.valueOneMayRemain = true;
    Rig s;
    s.acquire(ALARM);
    L::ActionContext done;
    s.prepare(done, false);
    s.commands(done, false);
    s.cycle(done);
    CHECK(done.state == M::ActionState::SUCCEEDED);
    done.valueOneMayRemain = true; // Forged on a success.
    CHECK(!L::nextAction(done, s.reservation, s.current, s.now, work));
}

// Cancellation and deadline races around the write in flight.
void cancellationAndDeadlineRaces() {
    // Cancel while the 1 is in flight; its ACK arrives anyway. The 0 is never sent.
    {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const unsigned from = r.wire.sentCount;
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        CHECK(L::noteActionWithdrawal(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
        CHECK(!r.next(action).ready);
        r.wire.push(reply(work.transfer), r.now + 1);
        r.turn(r.now + 1, budget(8, 0, 8, 0));
        CHECK(L::advanceAction(action, r.reservation, r.current, r.result(id), r.now));
        CHECK(action.outcome == M::ActionOutcome::CANCELLED &&
              action.execution == M::ActionExecution::ACKNOWLEDGED && action.valueOneMayRemain);
        r.turn(r.now + 1000);
        r.onlyActionTraffic(from, CLEAR, 1);
    }
    // A withdrawal noted at the cutoff is the deadline.
    {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        r.command(action, CLEAR, 1);
        const L::PreparedAction work = r.next(action);
        const O::RequestId id = r.admit(work.transfer);
        r.turn(r.now);
        r.turn(r.deadline, O::ServiceBudget());
        CHECK(L::noteActionWithdrawal(action, r.reservation, r.current, M::ActionOutcome::CANCELLED, r.now));
        CHECK(action.withdrawal == M::ActionOutcome::DEADLINE);
        r.turn(r.now, budget(8, 0, 8, 0)); // The 0's response budget ran out long ago.
        CHECK(L::advanceAction(action, r.reservation, r.current, r.result(id), r.now));
        CHECK(action.outcome == M::ActionOutcome::DEADLINE && action.valueOneMayRemain);
    }
    // A local DEADLINE before the cutoff is refused; at the cutoff it ends the attempt.
    {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        const Saved<L::ActionContext> fresh(action);
        CHECK(!L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::DEADLINE, r.now));
        fresh.unchanged(action);
        r.turn(r.deadline, O::ServiceBudget());
        CHECK(L::advanceAction(action, r.reservation, r.current, M::ActionOutcome::DEADLINE, r.now));
        CHECK(action.outcome == M::ActionOutcome::DEADLINE && !action.valueOneMayRemain &&
              action.execution == M::ActionExecution::NOT_TRANSMITTED);
    }
}

// A configuration or lifecycle change after preparation makes the frozen work unusable: every
// call refuses atomically until the application ends it INVALIDATED.
void staleConfigurationAfterPreparation() {
    for (unsigned which = 0; which < 2; ++which) {
        Rig r;
        r.acquire(ALARM);
        L::ActionContext action;
        r.prepare(action, false);
        r.command(action, CLEAR, 1);
        L::ReadTarget moved = r.current;
        if (which)
            ++moved.nodeGeneration;
        else
            ++moved.channel.binding.configurationGeneration;
        const Saved<L::ActionContext> before(action);
        L::PreparedAction work;
        const Saved<L::PreparedAction> blank(work);
        CHECK(!L::nextAction(action, r.reservation, moved, r.now, work));
        blank.unchanged(work);
        CHECK(!L::noteActionTransmission(action, r.reservation, moved, r.now));
        CHECK(!L::advanceAction(action, r.reservation, moved, M::ActionOutcome::CANCELLED, r.now));
        before.unchanged(action);
        CHECK(L::advanceAction(action, r.reservation, moved, M::ActionOutcome::INVALIDATED, r.now));
        CHECK(action.outcome == M::ActionOutcome::INVALIDATED && action.valueOneMayRemain &&
              action.commandEvidenceMask == 1);
    }
}
} // namespace
int main() {
    admissionScope();
    modeThreeAdmission();
    completionNeedsTheAdmissionMode();
    trialFiveThreeTwoThree();
    controlwordMethod();
    incidentShapeIsChecked();
    staleCodeWithLaterIncident();
    latchedErrorValue();
    methodsDoNotCross();
    prefixFailureAtEveryStage();
    delayedFinalZeroAck();
    rebootDuringClear();
    emcyDuringClear();
    if (PAIRED_CHANNELS)
        twoNodeIsolation();
    valueOneFlagIsChecked();
    cancellationAndDeadlineRaces();
    staleConfigurationAfterPreparation();
    std::printf("alarm clear: %u checks passed\n", checks);
}
