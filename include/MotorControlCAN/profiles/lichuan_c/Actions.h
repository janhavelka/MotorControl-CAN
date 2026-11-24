/** @file Actions.h
 * @brief Finite source-backed 86C stop, fault-clear, device-zero, enable, mode-select and release actions.
 * SPDX-License-Identifier: MIT
 * Stop transition contract: CL86-C/OL86-C only, one continuous actual owner reservation
 * acquired before the last positively acknowledged 6040:00 U16 word. Fresh SDO
 * STATUS (mode 1, 3 or 6, low bits 7) and MONITORS (alarm/released clear)
 * must follow that acknowledgement. The application proves no intervening
 * control/mode/target/RPDO/configuration write, uncertain work or lifecycle change.
 * A copied or reacquired reservation cannot establish this continuous history. The
 * library cannot see that history, so the application's session must supply it
 * (see examples/common/AxisSession.h): a successful stop consumes the prior
 * eligibility even if its copied reservation remains available.
 * Allowed predecessors (DEC-8): 000F documented, EN86/ZH86 p12 and OP86 p10 for 000B,
 * ZH86 p12 and OP86 p10 only for 010F. 001F/005F/003F/007F, and 010F to 000B only, are
 * project decisions with no documented arrow, so predecessorDocumented is false.
 * Those arrows appear in the position section. Mode 3 admission accepts only latest acknowledged
 * 000F with running set, as DEC-8 project policy; predecessorDocumented stays false. Mode 1 permits
 * running either set or clear. An explicit mode 3 HALT-to-QUICK_STOP escalation requires its own
 * acknowledged 010F and a later mode 3 running observation.
 * Mode 6 HALT accepts only acknowledged 001F and emits 000F (bit4 falling), with
 * predecessorDocumented=false. Fresh mode 6 monitor-not-running plus zero 606C settles
 * its controller observation; it proves no physical reference or standstill.
 * Other modes: HALT emits only 010F; QUICK_STOP emits only 000B. Neither enables first. A prior
 * 000B, a prior 010F for HALT, any other word and a released state are rejected.
 * After ACK, fresh MONITORS+STATUS cycles (plus FEEDBACK in mode 6) require the admitted mode and no
 * alarm/release; monitor running clear establishes OBSERVED controller-not-running
 * during that sequential read window. The one exception is QUICK_STOP's drive-reported
 * terminal: on both specimens and one firmware, 000B from 000F and from 010F left 2100
 * released with 6041 low bits 000 (docs/stop_actions.md), so that report completes a
 * QUICK_STOP, and monitorObservation keeps the release: the drive reports itself released,
 * and no torque was observed. During a move the same release kept 2100 running set
 * (2026-11-04), so a QUICK_STOP then completes on standstill instead
 * (ActionContext::standstillObserving). Those exceptions apply in mode 1, and in mode 3 only to a
 * QUICK_STOP from 000F that followed a running report and whose maximumAgeUs covers seven fields
 * (backlog S18, trials 1513 and 1607): it reads FEEDBACK from its first cycle, and a release completes
 * it only after two released cycles in a row read 606C zero and 6064 unchanged, never on one report.
 * Otherwise mode 3 requires running, alarm and release clear by project policy, not firmware evidence. Other
 * mode/alarm/release is outside the observation scope, not proof that physical stopping
 * failed. Other statusword dialect bits never decide completion. No observation read starts once
 * one exchange's queue and response budgets no longer fit before the deadline: the action then
 * ends at its deadline with nothing in flight. No 605A/6085, guessed 605D, queue disposition,
 * torque/reference retention, shaft standstill or firmware qualification is claimed.
 * One owner must retire obsolete queued/controller work before stop admission.
 * BestEffortStop is the separate DEC-2 path for an unresolved route or missing history.
 * Independently received EMCY/fault/lifecycle withdrawal must be delivered as a
 * local terminal before accepting later observation completions; an older monitor
 * read never overrides a newer routed fault. While admitted work is still in flight, deliver it
 * with noteActionWithdrawal instead: that work's terminal is then consumed as evidence, it cannot
 * complete or advance the action, and the outcome is the withdrawal's reason.
 * Enable transition contract: EN86/ZH86 p12 and OP86 p9 document 6040 words 6, 7, 15 about 10 ms
 * apart in position mode. EnableGate::MEASURED (prompt 57) makes the mode 1 enable a regular command,
 * opened by the reviewed record of 111 successful enables on two CL86-C specimens, one firmware: its
 * start is the fresh report alone, the measured power-up report or the released standstill, with no
 * trial identity, retained stop record or per-boot word history. EnableGate::ATTENDED_EXPERIMENT stays
 * for unmeasured states and mode 6, with the caller's explicit statements; a closed gate refuses. Mode 6
 * also requires its own evidence flag and a non-enabled released start; it sends no homing trigger.
 * Mode 3 startup belongs to the separately gated finite velocity operation. Release (prepareRelease) has
 * its own measured table, below.
 * Both specimens read 6041 low bits 111 and 2100 released clear at power-up, the pattern the manual
 * pairs with an enabled drive, and both held torque then by hand checks: word 6 releases it
 * (2026-11-03). At word 7 the drive reports itself energized again, and its position report returned
 * to its last commanded value (trial 308, 2026-11-04; the shaft was not observed). A released start
 * (EnableStart::RELEASED), the running latch of a moving quick stop included, records the word 7
 * position move, claims no continuity across it, and fails on any position or velocity change after
 * word 15, which the caller must answer with a stop. Read docs/drive_actions.md for the full table.
 * Mode select contract (prepareModeSelect): one 6060:00 byte of a documented code, 1, 3 or 6, from a
 * fresh report of a drive that is not enabled, or from the measured power-up report to mode 1, and
 * settled only when a later fresh 6061 reads that code. It never reads 6060 (write-only in the EDS,
 * D04), never writes 6061, never enables and is never resent. Other codes stay in the D17 probe.
 * Preparation and advancement validate first, then write the caller's context in place,
 * so no call copies a context onto the stack and a refused call changes nothing. Keep
 * contexts in static or owner storage, never in task locals.
 */
#pragma once
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** The manual's "about 10 ms" between enable words (EN86/ZH86 p12, OP86 p9), in microseconds. It is
 * the shortest spacing admitted, as project policy: no source gives a tolerance. It is an interval
 * to honor, not a settling time, so reaching it establishes nothing about the drive. */
constexpr uint64_t ENABLE_WORD_INTERVAL_US = 10000;
/** Services one enable schedules at least: the 607A and 60FF uploads, three words and one
 * seven-field cycle after each word. prepareEnable needs a deadline beyond three intervals plus this
 * many queue-and-response budgets. Polls beyond one cycle per word are not counted. */
constexpr uint64_t ENABLE_SERVICE_STEPS = 26;
/** Exact documentary native command selection, without generic deceleration promises. */
enum class StopKind : uint8_t {
    HALT, ///< Modes 1/3: 010F; mode 6: 000F bit4 falling from acknowledged 001F (derived word).
    QUICK_STOP ///< EN86/ZH86 p12 and OP86 p10 000F to 000B example, no certified safety claim.
};
/** Caller policy, not manufacturer response/stop timing. Defaults intentionally reject. */
struct ActionOptions {
    ReadOptions service;                ///< Nonzero queue/response budgets for each service.
    uint64_t observationIntervalUs = 0; ///< Nonzero delay between complete observation cycles.
    uint64_t maximumAgeUs =
        0; ///< Nonzero inclusive earliest-bound age cap for prerequisite/result blocks.
    /** ENABLE only: least time from one word's acknowledgement to the next word's eligibility, at
     * least ENABLE_WORD_INTERVAL_US. Other actions ignore it. A later word is also held for that
     * step's observation, so the actual spacing can be longer; commandHistory keeps the times. */
    uint64_t commandIntervalUs = 0;
};
/** Concrete action's active phase. */
enum class ActionPhase : uint8_t {
    COMMAND,   ///< Current ordered native download pending; never replay it.
    OBSERVING, ///< One finite action-specific state read cycle pending.
    WAITING,   ///< Completion not yet reported; next cycle waits until eligibleUs.
    TARGETS    ///< ENABLE only: the single 607A and 60FF uploads, before any 6040 word.
};
/** Which sources show a stop's whole-word arrow. OP86's body carries a retained 57C/86C
 * heading conflict, and no source is firmware evidence for a specimen. */
enum class PredecessorBasis : uint8_t {
    NONE,             ///< Not a stop context.
    EN86_ZH86_OP86,   ///< 000F to 000B: EN86 and ZH86 p12, OP86 p10.
    ZH86_OP86,        ///< 000F to 010F: ZH86 p12 and OP86 p10; EN86 omits the row.
    PROJECT_DECISION  ///< DEC-8: word or mode without a documented arrow.
};
/** Owned yielded service, not admission or transmission permission after epochs change. */
struct PreparedAction {
    CANOPEN::SdoTransfer transfer; ///< Complete copied service when ready.
    bool ready = false;            ///< False for terminal or not-yet-eligible work.
    uint64_t eligibleUs = 0;       ///< Earliest caller processing time for this work.
};
/** Which documented reset an alarm clear writes (Q13). Both share prepareClearAlarm's admission, its
 * cycles, its completion rule and its one final ERRORS read; only the two downloads differ. Each writes
 * its second value only after the first one's acknowledgement, never repeats a write and never writes
 * any other object or value. No source says which of the two a firmware honors, or whether they are
 * equivalent, and none states a hold time between the two writes.
 *
 * CONTROLWORD_BIT7 is prepareClearAlarm's default since 2026-11-13, a project decision from the bench
 * (docs/reports/2026-11-13_velocity_limits_and_fault_recovery.md): two CL86-C specimens of one firmware
 * (1000=00040192, 1009=000C, 100A=0020), FF05 raised by mode 3 velocity runs at the speed or
 * acceleration limit, and no other alarm. There CONTROLWORD_BIT7 cleared the alarm and left the drive
 * released (6041=8050, 2100=0011) 11 of 11 times, on both specimens. OBJECT_2301 cleared it 8 of 9
 * times but left 6041=8010 with 2100=0001, not released, which no S2 bench route admits and which
 * only a later 6040 0x0080 released; once, at 2201=51200, the FF05 came back right after the 2301
 * clear. Mode 1 clears, other alarms, firmwares and models are unmeasured for either method.
 */
enum class AlarmClearMethod : uint8_t {
    /** 2301:00 U16 1, then 0: EN86 PDF p18 "Alarm reset, 0: Noneffective; 1: Effective", U16 RW, and
     * EDS Obj.ClearAlm, U16 RW, PDO, default 0. ZH86 p18 and OP86 omit 2301. The 0 is library policy
     * (D-27). prepareClearAlarm's default until 2026-11-13; still selectable by name. Ran 9 times on
     * 2026-11-13, first node 1, mode 3, trial 5323 (see prepareClearAlarm): on its own it can leave
     * 6041=8010 with 2100=0001, which needs a further 6040 0x0080 that this method never writes. */
    OBJECT_2301,
    /** Default. 6040:00 U16 0x0080, then 0x0000. EN86 and ZH86 PDF p15 and OP86 p13, "3.5.2 Alarm
     * reset": set bit 7 of 6040 to 1 to reset the drive alarm, "It should be set to 0 manually"; ZH86
     * p12 and OP86 p10 list the step 15->128 "alarm reset" in the position-mode word table, which EN86
     * p12 lacks. 0x0000 is that manual return. No source gives a predecessor other than 15, so sending
     * 0x0080 after whatever word the drive last took is project policy: this action never sends the 15,
     * which would be an enable. Both words have bits 0..3 clear, which the p12 tables pair with 6041 low
     * bits 000 (6040=0), so on a drive reporting itself switched on they may also release it
     * (inference; every measured clear was admitted already released). Measured as above: 6041=8050 and
     * 2100=0011 after the clear. The action reads only after the second word, so these records do not
     * show which word acted, and whether bit 7 acts on its rising edge or its level is unknown. Neither
     * word is an allowed stop predecessor (stopPredecessorAllowed). */
    CONTROLWORD_BIT7
};
/** What ties the admission's 603F code to the alarm that 2100 reported in the same read. */
enum class AlarmCodeLink : uint8_t {
    NONE,      ///< Not a CLEAR_ALARM context.
    /** The only value admission produces. 603F read FF05 and 2100 reported an alarm, but no source
     * ties the code to that alarm, and 603F may keep an earlier code (Q13, D-26). So the alarm being
     * cleared may be another event than the FF05 one, and may not be a resettable class at all. */
    UNRESOLVED
};
/** What a clear attempt set out to clear: raw prerequisite values, as read at admission.
 * Later cycles replace the context's observations, so this keeps the incident. Per-field
 * provenance is not kept: all four blocks share the reservation's route and node epoch,
 * and earliestUs/latestUs bound them together. The application keeps its full read if it
 * needs each field's times. No value here is an interpretation or a reset claim.
 */
struct AlarmIncident {
    bool present = false;          ///< True only in a CLEAR_ALARM context.
    /** Always UNRESOLVED in a CLEAR_ALARM context: the identity of the reported alarm is unknown. */
    AlarmCodeLink codeLink = AlarmCodeLink::NONE;
    uint32_t readOperation = 0;    ///< Operation of the read that established the prerequisite.
    uint64_t earliestUs = 0;       ///< Earliest observation bound across the four blocks.
    uint64_t latestUs = 0;         ///< Latest observation bound across the four blocks.
    uint8_t errorRegister = 0;     ///< ERRORS: 1001 raw.
    uint16_t errorCode = 0;        ///< ERRORS: 603F raw, FF05 at admission; possibly an earlier event's.
    uint8_t errorCount = 0;        ///< ERROR_HISTORY: active entries, 0..4.
    uint32_t errorHistory[4] = {}; ///< ERROR_HISTORY: native error values, layout undocumented.
    /** MONITORS: 2100 raw. Running set here only in mode 3, beside released: the latched release,
     * after which later cycles may keep reporting running while they still report the alarm. */
    uint16_t monitor = 0;
    uint16_t logicalInputs = 0;    ///< MONITORS: 2101 raw.
    uint16_t logicalOutputs = 0;   ///< MONITORS: 2102 raw.
    uint16_t statusword = 0;       ///< STATUS: 6041 raw.
    /** STATUS: 6061 complete byte, 1 or 3; every cycle of the attempt must repeat it. */
    uint8_t modeRaw = 0;
};
/** SET_DEVICE_POSITION only: the raw feedback read at admission, kept after later cycles replace
 * extraObservation. When startPosition was already zero, a later raw zero shows no change at all.
 * Neither value says which internal counter 2302 clears (Q14).
 */
struct CounterBaseline {
    bool present = false;       ///< True only in a SET_DEVICE_POSITION context.
    int32_t startPosition = 0;  ///< FEEDBACK: 6064 raw at admission, no scale or home meaning.
    int32_t startVelocity = 0;  ///< FEEDBACK: 606C raw at admission; admission requires zero.
    uint64_t earliestUs = 0;    ///< Earliest bound of that FEEDBACK block.
    uint64_t latestUs = 0;      ///< Latest bound of that FEEDBACK block.
};
/** Whether enable may run at all. This is the capability gate, not a readiness claim. Appended values
 * keep the earlier numbers. */
enum class EnableGate : uint8_t {
    CLOSED,             ///< Default: prepareEnable returns UNSUPPORTED with no output change.
    /** One attended trial with an independent drive-power cutoff. Two specimens ran it attended on
     * one firmware (docs/drive_actions.md); that qualifies no firmware or capability. It keeps every
     * statement EnableEvidence carries and stays the gate for states outside MEASURED, mode 6 included. */
    ATTENDED_EXPERIMENT,
    /** The regular mode 1 enable (prompt 57), opened by the reviewed record of the retained enables:
     * 111 succeeded on both CL86-C specimens, one firmware (1000=00040192, 1009=000c, 100A=0020), 28 of
     * the 30 attended records 103 to 2402 and 83 night-soak runs (docs/bench/20261111-night/night.log);
     * records 103 and 105 (2026-11-03, node 1) failed before the bit 10 fix and after a hand displacement.
     * Its measured starts are the power-up report 8437/0001 in mode 1 (21 records: word 6 8031/0011,
     * word 7 8033/0001, word 15 8437/0001) and the released standstill a quick stop leaves (8050/0011,
     * the latches 4050/0014 and C050/0015); 8031/0011, the report word 6 leaves, is the released
     * standstill's next state on that measured path. The start is the fresh report alone: the library
     * classifies it (EnableRecord::start), and trial, expiresUs, reservation, powerUpShaft and the
     * reviewed values are not consulted. 607A and 60FF are still read before any word and kept, not
     * compared: no mode 1 enable started motion from a retained target. Mode 1 only. Every observation
     * rule of the sequence is unchanged. It qualifies no other firmware or model. */
    MEASURED
};
/** Which start an enable runs from. Under ATTENDED_EXPERIMENT the caller states it; under MEASURED the
 * fresh report decides it (EnableRecord::start), and a caller's RELEASED or POWER_UP only requires that
 * class. Appended values keep the earlier numbers. */
enum class EnableStart : uint8_t {
    /** Default. ATTENDED_EXPERIMENT refuses it: fresh flags cannot tell idle from an interrupted curve.
     * MEASURED accepts it as "either measured start". */
    UNKNOWN,
    /** ATTENDED_EXPERIMENT: no 6040, target, ramp or other mode write reached this drive since its last
     * power-up, as far as this application's retained record can show, apart from one acknowledged 6060
     * selecting this explicitly reviewed mode. A statement the library cannot check, and not proof that
     * the drive holds no pending curve. MEASURED treats it as POWER_UP: the statement is not needed. */
    NO_MOTION_WRITE_RECORDED,
    /** The released standstill, for drives that stay powered between runs (2026-11-10). Since prompt 57
     * the start is the fresh report alone: no retained stop record and no statement about earlier
     * writes is asked (the recorded-stop premise of 2026-11-10 is gone); the application owns what it
     * knows beyond the report. Mode 1 only.
     *
     * The start report must be a released standstill: 2100 released, alarm, both soft limits and every
     * undocumented bit clear, the in-place bit set or clear, 2100 running set or clear; 6041 bits 0..6
     * 0x50, the pattern of every measured release (low bits 000), or 0x31, the report word 6 leaves (low
     * bits 001, 8031/0011); bits 14 and 15 as reported; 1001, 603F and 606C zero. Running set beside the
     * release is the latch a quick stop during a mode 1 move leaves (2100=0014, 6041=4050, trial 2109
     * after 2108; 2100=0015, 6041=C050 in mode 3, trial 1924), and 6041 bit 14 was set exactly while it
     * was latched. From 0x31 word 6 repeats the word that led there, which no record measured as a
     * repeat; its cycles must still report low bits 001 and the release.
     *
     * Words 6, 7 and 15 follow as for NO_MOTION_WRITE_RECORDED, with these differences, each measured
     * from released starts on one firmware or chosen for them here.
     * - 6041 bits 5 and 6 may change: from every release so far the word 6 cycle moved bits 0..6 from
     *   0x50 to 0x31 (trial 1782 in mode 3, 8050 to 8031; trial 308 in mode 1, 4050 to 4031).
     * - The latch, 2100 running and 6041 bit 14, may stay as admitted or clear in word 6's cycles, and
     *   must read clear in every cycle from word 7 on: word 7 cleared it in trials 308 (4031/0014 to
     *   0033/0000) and 1742 (C031/0015 to 8033/0001). A latch still reported then ends the sequence,
     *   word 15 unsent, with the drive switched on and that cycle kept in the observations. No other
     *   2100 bit, the in-place bit included, may change.
     * - 6064 must repeat the admission value in word 6's cycles. Word 7 energizes the drive and its
     *   position report moves to the drive's internal command (mode 3: -6 to +12 counts in trials 1742 to
     *   1862; mode 1: 1187 to 1194 to 1180 over half a second in trial 308, +1 in trial 2102), with no
     *   bound known, so word 7's cycles are not compared. Word 15 waits instead until two cycles in a
     *   row after word 7 report low bits 011 and the same 6064; that cycle is word 7's accepted one.
     * - From word 15 on, every cycle must repeat word 7's accepted 6064, with 606C zero. The drive
     *   retains its target, an interrupted move's included (after trial 2108 607A read -2000, the
     *   relative displacement from -1522, so that move's target was -3522), and whether words 6, 7
     *   and 15 resume it is unmeasured (a 000F rearm after a HALT did not, trials 829 and 2107). A
     *   change ends the sequence with ActionOutcome::OBSERVATION_SCOPE and a status message saying
     *   so; the drive may then be moving and enabled, so the caller requests a QUICK_STOP at once.
     *   After an acknowledged word 15 the session's latest controlword is that 000F, which a stop
     *   follows (prepareStop needs a fresh report with low bits 111, not released and no alarm).
     *
     * EnableRecord keeps 6064 at admission and in each word's accepted cycle. Success claims no position
     * continuity across word 7: a later move takes its start position from a fresh read. */
    RELEASED,
    /** Deprecated name of RELEASED, kept for source compatibility: it no longer asks for a recorded stop. */
    RELEASED_BY_RECORDED_STOP = RELEASED,
    /** MEASURED only: the measured power-up report, mode 1 with 6041 low bits 111 and 2100 in place with
     * released, running, alarm, both soft limits and every undocumented bit clear (8437/0001 on both
     * specimens). The record states that report holds torque on this model and word 6 removes it, so no
     * shaft observation is asked. The same report after a word 15 cannot be told apart; the sequence is
     * the one measured from it either way. ATTENDED_EXPERIMENT refuses it as an argument. */
    POWER_UP
};
/** The attendant's own observation of the shaft after power-up, before any word. The library cannot
 * check it. Under ATTENDED_EXPERIMENT a start report matching the manual's enabled pattern needs one of
 * the two values. It is not consulted when the start report reads 2100 released, as every RELEASED start
 * does, nor under MEASURED. */
enum class ShaftObservation : uint8_t {
    NONE,   ///< Default: not observed. Refused when the start report matches the enabled pattern.
    FREE,   ///< Turned freely by hand: no holding torque observed.
    HOLDING ///< Resisted by hand: holding torque observed, which word 6 may then remove.
};
/** The caller's explicit evidence for one enable. Defaults are refused. Under EnableGate::MEASURED only
 * gate, modeRaw (1), operation and an optional expected start are consulted.
 * The reviewed values are read and compared, so they cannot go stale silently, but a match means only
 * that someone looked at these exact bits. No value is thereby called safe, known or intended. Both
 * targets need review whatever they read, zero included: an absolute target of zero is not a
 * no-motion claim, and 60FF=0 has no documented meaning.
 */
struct EnableEvidence {
    uint8_t modeRaw = 1; ///< Explicit reviewed mode 1 or 6; no mode write is emitted by enable.
    bool homingEnableQualified =
        false; ///< Mode 6 only: retained trial evidence establishes no start
               ///< before a separate bit-4 edge on this firmware/binding.
    EnableGate gate = EnableGate::CLOSED;     ///< Must be explicitly opened: MEASURED or an attended trial.
    /** Nonzero identity of the attended trial record. AxisSession consumes each once, in rising order,
     * for the life of the session object. */
    uint32_t trial = 0;
    /** The exact reservation, and so the exact binding, this trial was reviewed for. A copy made under
     * another reservation, as after an MCU restart or by another session, does not match it. */
    AxisReservation reservation;
    /** The exact operation identity this enable runs under. Owner operations only rise on a
     * reservation, so the evidence opens at most one enable there, whichever session holds it. */
    uint32_t operation = 0;
    uint64_t expiresUs = 0; ///< Exclusive cutoff of this evidence in the caller's clock domain.
    EnableStart start = EnableStart::UNKNOWN; ///< Application's record of the drive's prior history.
    ShaftObservation powerUpShaft = ShaftObservation::NONE; ///< Attendant's observation at power-up.
    bool reviewed = false;                    ///< The three reviewed fields below are meaningful.
    uint16_t reviewedStatusword = 0;  ///< Exact 6041 at admission. Disputed bits are never interpreted.
    int32_t reviewedPositionTarget = 0; ///< Exact 607A read before any word. History, not intent.
    int32_t reviewedVelocityTarget = 0; ///< Exact 60FF read before any word. History, not intent.
};
/** One enable word's accepted observation, retained after later cycles replace the blocks. */
struct EnableStepRecord {
    bool present = false;     ///< This step's expected report was accepted.
    uint16_t statusword = 0;  ///< 6041 raw of that cycle.
    uint16_t monitor = 0;     ///< 2100 raw of that cycle.
    int32_t position = 0;     ///< 6064 raw of that cycle, no scale.
    int32_t velocity = 0;     ///< 606C raw of that cycle, no scale.
    uint64_t earliestUs = 0;  ///< Earliest bound of that cycle, sequential and not atomic.
    uint64_t latestUs = 0;    ///< Latest bound of that cycle.
};
/** ENABLE history kept for inspection, including after failure. None of it is an interpretation. */
struct EnableRecord {
    EnableEvidence evidence;      ///< Copied caller evidence.
    /** Frozen identity comparison of the admission against measuredIdentity(): CONFIRMED, both tuples,
     * and firmwareUnqualified when the drive runs a firmware no reviewed enable record was made on. */
    IdentityCheck identity;
    /** The start the sequence runs under: the caller's under ATTENDED_EXPERIMENT; under MEASURED the class
     * the fresh report showed, POWER_UP or RELEASED. UNKNOWN in every other context. */
    EnableStart start = EnableStart::UNKNOWN;
    /** Admission 6041; bits other than 0..2 and 10 must not change during the sequence, nor, after a
     * RELEASED start, bits other than 0..2, 5, 6, 10 and 14; bit 14 may then only clear,
     * and must from word 7 on. */
    uint16_t startStatusword = 0;
    /** Admission 2100; bits other than released must not change. After a RELEASED
     * start running may also clear, and must from word 7 on. */
    uint16_t startMonitor = 0;
    /** Admission 6064, read before word 6. Any later raw difference ends the sequence. After a
     * RELEASED start a difference ends it in word 6's cycles, none is judged in word 7's,
     * and from word 15 on a difference from steps[1].position does. */
    int32_t startPosition = 0;
    uint8_t targetCursor = 0;     ///< Current upload: 0 is 607A, 1 is 60FF.
    bool positionTargetKnown = false; ///< positionTarget holds an acquired value.
    bool velocityTargetKnown = false; ///< velocityTarget holds an acquired value.
    int32_t positionTarget = 0;   ///< 607A as read before the first word; history, not intent.
    int32_t velocityTarget = 0;   ///< 60FF as read before the first word; history, not intent.
    /** Accepted observation after words 6, 7 and 15. After a RELEASED start, word 7's
     * is the second of the two still cycles word 15 waited for. */
    EnableStepRecord steps[3];
};
/** Which fresh report admitted a mode select. The library judges only that report: it keeps no
 * history of earlier writes, and the application owns what it knows beyond the report. */
enum class ModeSelectStart : uint8_t {
    NONE, ///< Not a SELECT_MODE context.
    /** The drive reports itself not enabled: 2100 bit 4 set (EN86 p17 and ZH86 p18, set released and
     * clear enabled) with 6041 bits 1 and 2 clear, low bits 000 or 001, as every released report of both
     * specimens read (8050, C050, 4050; 8031 after word 6). Any requested mode. */
    NOT_ENABLED,
    /** The measured power-up report: 6061 0, 6041 exactly 8037 and 2100 exactly 0001. Both specimens
     * report the manual's enabled pattern there; the 23 retained writes from it, records 102 to 1722 on
     * one firmware (1000=00040192, 1009=000c, 100A=0020), were all 6060=1, acknowledged and followed by
     * 6061=1 (6041=8437). So only mode 1 is admitted from it (lead decision, 2026-11-24): another
     * mode from the manual's enabled pattern is a mode change while enabled that nothing measured. */
    POWER_UP
};
/** SELECT_MODE only: the request and the admission report, kept after later cycles replace the blocks.
 * Raw values, never an interpretation. */
struct ModeSelectRecord {
    ModeSelectStart start = ModeSelectStart::NONE; ///< The admitted start; NONE in every other context.
    uint8_t requestedModeRaw = 0; ///< The one 6060 byte written: 1, 3 or 6.
    /** 6061 complete byte at admission; differs from requestedModeRaw unless alreadySelected. */
    uint8_t startModeRaw = 0;
    uint16_t startStatusword = 0; ///< 6041 raw at admission.
    uint16_t startMonitor = 0;    ///< 2100 raw at admission.
    int32_t startPosition = 0;    ///< 6064 raw at admission; every cycle must repeat it.
    /** True when the admission report, a NOT_ENABLED start, already showed 6061 equal to the request: the
     * context ended SUCCEEDED at preparation with execution NOT_TRANSMITTED, and no write was needed or
     * sent, as the release table's already-released row answers. */
    bool alreadySelected = false;
};
/** The rows of the release table (prompt 57), the one place the release's admission, its settlement and
 * prompt 72's guard read. No manual names a release word: EN86/ZH86 p12 and OP86 p10 only pair 6040=0
 * with 6041 low bits 000 (OP86 p10 carries a CL57-C header). Each measured row's scope is two CL86-C
 * specimens on one firmware (1000=00040192, 1009=000c, 100A=0020). A row is chosen by the fresh report
 * (6041, 2100, 606C, 1001, 603F, 6061, and 2500 for row 2), never by retained history; row 4 comes before
 * row 5. */
enum class ReleaseRow : uint8_t {
    NONE, ///< Not classified; every context but RELEASE.
    /** Row 1: mode 1, 6041 low bits 111, 2100=0001 (in place; released, alarm and running clear), 606C,
     * 1001 and 603F zero: the power-up report 8437/0001. Word 0x0006, settled by low bits 001 with 2100
     * released (8031/0011). Measured as the first word of 21 power-up enables, both specimens (node 1
     * trials 108 to 1703, node 2 trials 118 to 1723, docs/bench/ *-p3-node*); node 1's shaft then turned
     * freely by hand (2026-11-03). The same report after this library's own word 15 cannot be told apart
     * and is admitted on it. */
    POWER_UP_IDLE,
    /** Row 2: mode 3, low bits 011, 2100=0001, 606C, 1001 and 603F zero, and a fresh 2500=1 (open loop):
     * 8033/0001, as a velocity enable refused at word 7 leaves it. Word 0x0000, settled by low bits 000
     * with 2100 released (8050/0011). Measured once: node 2, trial 5991 (docs/bench/20261113-ladder/
     * night.log:8763-8800, packed/S0193-n2-disable.tar.xz), about 15 minutes after the refusal of trial
     * 5976. Trial 5991 read no 2500: its open loop is inferred from history (written and read back by
     * trial 5973), so the library requires a fresh 2500=1 here. That read gave 606C 1711 beside the
     * release, inferred not a rotor speed. */
    SWITCHED_ON_OPEN_LOOP,
    /** Row 3: mode 3, low bits 000, 2100=0001 (released clear), 606C, 1001 and 603F zero: 8010/0001, as an
     * OBJECT_2301 alarm clear leaves it. Word 0x0080 alone, settled by low bits 000 with 2100 released
     * (8050/0011). Measured on node 1: archives packed/20261113T141445Z-cw-0080-n1-5405 after
     * ...141347Z-cw-0000-n1-5402, ...141402Z-cw-000b-n1-5403 and ...141417Z-cw-0006-n1-5404 changed nothing,
     * then 8 of 8 (trials 5362, 5396, 5452, 5500, 5524 on node 1 and 5632, 5650, 5668 on node 2). It is the
     * fault-reset bit (ZH86 p12 and OP86 p10 15->128; EN86/ZH86 p15), the measured exit from this report
     * only, never a release word elsewhere. */
    ALARM_CLEARED,
    /** Row 4: 2100 released set with 6041 low bits 000 or 001 (8050/0011, the latches 4050/0014 and
     * C050/0015, 8031/0011), in any mode and whatever its running bit or an alarm: already released, nothing
     * sent, the running, alarm and fault facts reported in ReleaseDecision. */
    ALREADY_RELEASED,
    REFUSED, ///< Row 5: every other report, refused before TX with its ReleaseRefusal.
    /** An explicit ReleaseExperiment word, 0x0000 or 0x0006, from a report outside the measured rows or with
     * a word other than its row's; settled like a row, by the p12 table's low bits for that word (000 for
     * 0, 001 for 6) with 2100 released. Unmeasured: the result is an experiment, never a measured row. */
    EXPERIMENT
};
/** Why the release table refused a report (row 5). The experiment option may override only MOVING, MODE,
 * LOOP and PATTERN, the unmeasured refusals; never ALARM (the alarm clear's case), RUNNING (a stop's case)
 * or RELEASED. */
enum class ReleaseRefusal : uint8_t {
    NONE,     ///< Not refused.
    ALARM,    ///< 2100 alarm set, or 1001 or 603F nonzero.
    RUNNING,  ///< 2100 running set without the release: a moving drive needs a stop.
    RELEASED, ///< 2100 released set beside 6041 low bits other than 000 and 001: unmeasured, never sent to.
    MOVING,   ///< 606C nonzero with 2100 running clear, as in backlog S21.
    MODE,     ///< A measured row's 6041 low bits and 2100=0001 in another mode, such as the halted 9437/0001.
    LOOP,     ///< Row 2's report without a fresh 2500=1: closed loop, another selector or no 2500 read.
    PATTERN   ///< Any other report, such as S20's 1437/0000 or the mode 1 report 0037/0000.
};
/** The fresh report the release table judges: raw values of one read, no interpretation. */
struct ReleaseReport {
    uint8_t modeRaw = 0;       ///< 6061 complete byte.
    uint16_t statusword = 0;   ///< 6041 raw.
    uint16_t monitor = 0;      ///< 2100 raw.
    uint8_t errorRegister = 0; ///< 1001 raw.
    uint16_t errorCode = 0;    ///< 603F raw.
    int32_t velocity = 0;      ///< 606C raw, no scale.
    bool loopKnown = false;    ///< loopModeRaw holds a fresh 2500 read; needed for row 2 only.
    uint16_t loopModeRaw = 0;  ///< 2500 raw: 1 open loop, 2 closed loop (Tuning.h LoopMode).
};
/** The caller's explicit, unmeasured release experiment, off by default. When enabled, word (0x0000 or
 * 0x0006, nothing else) is sent from a report outside the measured rows whose refusal is MOVING, MODE,
 * LOOP or PATTERN, or from a row 1 to 3 report whose row names another word. The result is marked as an
 * experiment; only a reviewed measured result adds a row. */
struct ReleaseExperiment {
    bool enabled = false; ///< Explicitly requested by the caller.
    uint16_t word = 0;    ///< 0x0000 or 0x0006; never 0x0080 outside row 3, never bits 0..2 all set.
};
/** The release table's answer for one report. */
struct ReleaseDecision {
    ReleaseRow row = ReleaseRow::NONE;                 ///< The row, EXPERIMENT for an experiment word.
    ReleaseRefusal refusal = ReleaseRefusal::NONE;     ///< Why the report is outside the measured rows.
    bool send = false;          ///< One 6040 word is to be written: rows 1 to 3 and EXPERIMENT.
    uint16_t word = 0;          ///< That word: 0x0006, 0x0000 or 0x0080.
    uint8_t settleLowBits = 0;  ///< 6041 bits 0..2 that settle it, beside 2100 released.
    bool running = false;       ///< 2100 running in the report; row 4 reports it.
    bool velocityNonzero = false; ///< 606C nonzero in the report; reported, never a standstill claim.
    /** 2100 alarm bit in the report. Row 4 reports it and still sends nothing; every other row refuses. */
    bool alarm = false;
    /** 1001 or 603F nonzero in the report. Row 4 reports it and still sends nothing, clearing nothing;
     * every other row refuses. */
    bool fault = false;
};
/** SELECT_MODE's sibling for RELEASE: the table's answer and the admission report, kept after later
 * cycles replace the blocks, and the settling cycle's 606C. Raw values only. */
struct ReleaseRecord {
    ReleaseDecision decision;        ///< At admission; row NONE in every other context.
    uint8_t startModeRaw = 0;        ///< 6061 at admission; every cycle must repeat it.
    uint16_t startStatusword = 0;    ///< 6041 at admission.
    uint16_t startMonitor = 0;       ///< 2100 at admission.
    int32_t startPosition = 0;       ///< 6064 at admission, no scale.
    int32_t startVelocity = 0;       ///< 606C at admission, no scale.
    bool loopKnown = false;          ///< loopModeRaw came from a fresh 2500 read.
    uint16_t loopModeRaw = 0;        ///< 2500 raw at admission.
    /** 606C of the cycle that settled the release, reported beside the released report (trial 5991 read
     * 1711). Never a standstill claim: a release says nothing about the rotor. */
    int32_t settledVelocity = 0;
    bool settledVelocityNonzero = false; ///< settledVelocity is nonzero.
};
/** Classify one fresh report by the release table, with an optional experiment. Pure: no I/O, no context.
 * Prompt 72's guard and prepareRelease share it.
 * @param[in] report Raw values of one fresh read.
 * @param[in] experiment Off by default; when enabled its word must be 0x0000 or 0x0006.
 * @param[out] output The answer, written only on OK; disjoint from the inputs.
 * @return OK with any row, REFUSED included; INVALID_CONFIG with ActionError::ARGUMENT for an enabled
 * experiment naming another word, or OVERLAP, with output unchanged.
 */
Status classifyRelease(const ReleaseReport& report, const ReleaseExperiment& experiment,
                       ReleaseDecision& output) noexcept;
/** Caller-owned concrete action context. Serialize access; do not edit active fields.
 * Histories are inspectable after terminal and survive failure. Inputs are borrowed
 * during calls only. Every yielded/advanced service needs the actual live reservation.
 */
struct ActionContext {
    AxisReservation reservation; ///< Frozen route, model and continuous exclusion.
    ActionKind actionKind =
        ActionKind::STOP;                   ///< Explicit operation; kind remains the stop selector.
    StopKind kind = StopKind::HALT;         ///< Explicit native selection.
    /** Set by successful preparation: MOTION_LIKE for prepareEnable; NOT_REPEATABLE for every
     * stop (prepareStop, prepareEscalation, prepareVelocityStop, prepareHaltedVelocityQuickStop),
     * for prepareClearAlarm (2301 or 6040 bit 7) and for prepareSetDevicePosition (2302), whose
     * value-1 writes are edges with undocumented repeat behavior, and for prepareModeSelect, whose
     * 6060 cannot be read back and whose effect on the drive beyond 6061 is undocumented (Q11). A new
     * stop is a new operation with its own identity, never a replay: QUICK_STOP may be prepared again
     * while its admission rules hold and sends only 000B, and prepareStop refuses HALT after an
     * acknowledged 010F. The library cannot see an unconfirmed earlier stop: the application withdraws its predecessor
     * evidence when a stop may have left (AxisSession does), since a HALT prepared from an older
     * 000F after an unconfirmed 010F would send 010F again. */
    RetryClass retryClass = RetryClass::NOT_REPEATABLE;
    ActionOptions options;                  ///< Immutable caller budgets.
    ActionState state = ActionState::EMPTY; ///< Overall phase, distinct from drive state.
    ActionPhase phase = ActionPhase::COMMAND; ///< Current concrete work phase.
    /** STOP only: frozen raw 6061 mode, 1 or 3; every complete result cycle and escalation must
     * match it. Mode 3 is DEC-8 project policy, not a documented stop arrow or qualification. */
    uint8_t stopModeRaw = 1;
    uint16_t predecessorWord = 0; ///< STOP only: acknowledged 6040 word this stop follows.
    bool predecessorDocumented = false; ///< STOP only: false for DEC-8 project-decision arrows.
    PredecessorBasis predecessorBasis = PredecessorBasis::NONE; ///< STOP only: which sources show it.
    bool finalErrorRead = false; ///< CLEAR_ALARM only: alarm reported clear; read ERRORS once.
    /** CLEAR_ALARM only: the reset its two downloads write. Every other context keeps OBJECT_2301, the
     * neutral value the context checks require; it is not prepareClearAlarm's default method. */
    AlarmClearMethod clearMethod = AlarmClearMethod::OBJECT_2301;
    /** QUICK_STOP only. Mode 1: set when a cycle after the command reported 2100 released and 6041 low
     * bits 000 with 2100 running still set, and options.maximumAgeUs covers a seven-field cycle.
     * Measured on 2026-11-04 (trial 213): after 000B during a 30 rpm move the drive released the
     * motor, which stopped within about 10 pulses, but kept running set while its target stayed
     * unreached. Every later cycle then also reads FEEDBACK, and two cycles in a row with 606C zero
     * and 6064 unchanged complete the stop. Mode 3 (backlog S18): set at preparation for 000B from
     * 000F after a running report, with the same age cover: prepareStop's fresh running admission, or
     * prepareVelocityStop for a run's own stop whose latest cycle reported running. Every cycle reads
     * FEEDBACK; a released cycle with low bits 000 stays in scope, and two such cycles in a row with
     * 606C zero and 6064 unchanged complete the stop, running set or clear. Measured on 2026-11-10:
     * 28 ms after 000B from a running mode 3 run both specimens read 2100=0014 and 6041=C050 (trials
     * 1513, 1607), later 0015 with 606C 0 and 6064 still (reads 1514, 1608). Never cleared. */
    bool standstillObserving = false;
    ActionExecution execution =
        ActionExecution::NOT_TRANSMITTED; ///< Effecting command evidence only.
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED; ///< Narrow report evidence only.
    ActionOutcome outcome = ActionOutcome::NONE; ///< Immutable first terminal cause.
    Status status;          ///< Local/native operation status, separate from health.
    uint32_t operation = 0; ///< Nonzero live operation identity.
    uint32_t step = 1;      ///< Nonwrapping identity across every ordered download and observation.
    uint32_t observations = 0; ///< Complete cycles, bounded by identity exhaustion.
    uint64_t startedUs = 0;    ///< Caller preparation time, zero valid.
    uint64_t servicedUs = 0;   ///< Last local mutation time.
    uint64_t deadlineUs = 0; ///< Immutable exclusive deadline; equality expires before publication.
    uint64_t eligibleUs = 0; ///< Earliest next-cycle admission time.
    uint64_t stepStartedUs = 0;     ///< Earliest admission for the current external service.
    /** A local reason recorded by noteActionWithdrawal while work was in flight. NONE otherwise.
     * Once set, nothing new is yielded, and the next consumed terminal ends the action with it. */
    ActionOutcome withdrawal = ActionOutcome::NONE;
    bool transmissionNoted = false; ///< Current download crossed possible-TX boundary.
    bool effectsPossible = false;   ///< At least one prefix command may have reached the drive.
    bool uncertain =
        false; ///< Unconfirmed command/read effects or missing supported action observation.
    CANOPEN::SdoCompletion controlEvidence; ///< STOP only: prior positively acknowledged word.
    /** Each ordered download terminal, including failure. The effecting command is slot 0
     * for a stop, an alarm clear (2301 value 1, or 6040 0x0080) or a mode select (its one 6060) and
     * slot 1 for a counter clear (value 1). In an enable
     * every slot is effecting: words 6, 7 and 15, and a prefix stays retained after failure. */
    CANOPEN::SdoCompletion commandHistory[3];
    uint8_t commandEvidenceMask = 0; ///< Bits identify retained commandHistory slots.
    /** Current download slot: stop and mode select 0, alarm 0..1, counter 0..2, enable 0..2. */
    uint8_t commandCursor = 0;
    AlarmIncident incident; ///< CLEAR_ALARM only: the prerequisite, kept after later cycles.
    CounterBaseline baseline; ///< SET_DEVICE_POSITION only: admission feedback, kept after later cycles.
    EnableRecord enable;    ///< ENABLE only: evidence, targets and per-step observations.
    ModeSelectRecord modeSelect; ///< SELECT_MODE only: the request and its admission report.
    /** RELEASE only: the table's answer, the admission report and the settling cycle's 606C. */
    ReleaseRecord release;
    Generation originalReferenceGeneration = 0; ///< Counter action's frozen confidence epoch.
    bool coordinatesInvalidated =
        false; ///< Counter trigger crossed possible-TX boundary; never undone.
    /** CLEAR_ALARM and SET_DEVICE_POSITION only, set when the action fails: its value-1 write may
     * have reached the drive and the 0 after it has no positive acknowledgement. The object may still
     * hold 1; for AlarmClearMethod::CONTROLWORD_BIT7, 6040 may still hold 0x0080, bit 7 set, which
     * EN86/ZH86 p15 say must be returned to 0 manually. What a held 1 does, and whether a later 1 is a
     * new edge, is undocumented (Q13, Q14, D-27). Never cleared, and nothing resends either value: a
     * later attempt is new intent. */
    bool valueOneMayRemain = false;
    CANOPEN::SdoCompletion
        observationEvidence; ///< Latest actual external read terminal, including failure.
    bool observationEvidencePresent = false; ///< Makes observationEvidence meaningful.
    ReadContext read; ///< Current finite raw SDO cycle, retaining acquired field provenance.
    StateObservation
        statusObservation; ///< Latest complete cycle STATUS; preserved on later failure.
    StateObservation
        monitorObservation; ///< Latest complete cycle MONITORS; sequential, not atomic.
    /** Selected by its block. CLEAR_ALARM: raw 1001/603F, the prerequisite until the final read.
     * SET_DEVICE_POSITION, ENABLE and SELECT_MODE: signed 6064/606C, the admission's until the first
     * cycle replaces it, no scale/home proof.
     * QUICK_STOP: the latest 6064/606C once standstillObserving is set; otherwise unused, as for
     * HALT.
     * The default block is ERRORS with fieldCount 0, so check fieldCount before reading it. */
    StateObservation extraObservation;
    bool observationKnown = false;        ///< All required blocks of a complete cycle are retained.
};
/** Best-effort stop context (DEC-2), keyed to a target because its route has no usable
 * reservation. It yields only 6040:00=000B and needs no state evidence. It never
 * reaches SUCCEEDED: state FAILED with STOP_SENT_UNCONFIRMED means a frame left the
 * controller, and nothing is known about the drive. An independent physical stop stays
 * mandatory. Serialize access.
 */
struct BestEffortStop {
    ReadTarget target;                      ///< Frozen route and node epoch at preparation.
    ReadOptions service;                    ///< Nonzero queue/response budgets.
    ActionState state = ActionState::EMPTY; ///< ACTIVE until terminal; never SUCCEEDED.
    ActionExecution execution =
        ActionExecution::NOT_TRANSMITTED; ///< UNKNOWN from owner admission until terminal proof of no TX; never ACKNOWLEDGED.
    ActionOutcome outcome = ActionOutcome::NONE; ///< Immutable first terminal cause.
    Status status;                 ///< Local status, separate from health.
    uint32_t operation = 0;        ///< Nonzero live operation identity.
    uint64_t startedUs = 0;        ///< Caller preparation time, zero valid.
    uint64_t servicedUs = 0;       ///< Last local mutation time.
    uint64_t deadlineUs = 0;       ///< Immutable exclusive cutoff, fixed at preparation.
    /** NOT_REPEATABLE, also after prepareBestEffortStop: another best-effort stop is a new
     * operation with its own identity, and it too yields only 000B. */
    RetryClass retryClass = RetryClass::NOT_REPEATABLE;
    bool admitted = false;         ///< The owner holds this work; only its terminal can settle it.
    bool effectsPossible = false;  ///< The frame may have reached the drive.
    CANOPEN::SdoCompletion commandEvidence; ///< Owner terminal, when commandEvidencePresent.
    bool commandEvidencePresent = false;    ///< Makes commandEvidence meaningful.
};
/** The DEC-8 table for mode 1: may this stop follow this latest acknowledged 6040 word?
 * @param[in] word Latest acknowledged controlword.
 * @param[in] kind Native stop selection.
 * @return True for an allowed predecessor, documented or a project decision.
 */
bool stopPredecessorAllowed(uint16_t word, StopKind kind) noexcept;
/** Check budgets for prepareStop or prepareEscalation, before any work is retired.
 * @param[in] options Caller policies.
 * @return True with nonzero budgets, a maximumAgeUs covering five fields, and DECLARED_U16
 * identity widths: an action's reads are state reads, which take no other setting.
 */
bool stopOptionsValid(const ActionOptions& options) noexcept;
/** Return a context to EMPTY in place. `context = ActionContext()` builds a stack temporary
 * of the whole context on the generated code we measured; this does not.
 * @param[out] context Context to reset; its retained history is gone.
 */
void resetAction(ActionContext& context) noexcept;
/** Report what prepareStop would refuse, changing nothing. For a caller that must know before
 * it moves other state, such as an owner operation, and only then writes the context.
 * @param[in] output The context prepareStop would write; read only for its overlap checks.
 * @param[in] reservation As prepareStop. @param[in] state As prepareStop.
 * @param[in] control As prepareStop. @param[in] kind As prepareStop.
 * @param[in] operation As prepareStop. @param[in] nowUs As prepareStop.
 * @param[in] deadlineUs As prepareStop. @param[in] options As prepareStop.
 * @return The status prepareStop would give. OK means prepareStop with these same unchanged
 * values cannot refuse.
 */
Status validateStop(const ActionContext& output, const AxisReservation& reservation,
                 const ReadContext& state, const CANOPEN::SdoCompletion& control, StopKind kind,
                 uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                 const ActionOptions& options) noexcept;
/** Report what prepareEscalation would refuse, changing nothing.
 * @param[in] output The context prepareEscalation would write; read only for its overlap checks.
 * @param[in] running As prepareEscalation. @param[in] operation As prepareEscalation.
 * @param[in] nowUs As prepareEscalation. @param[in] deadlineUs As prepareEscalation.
 * @param[in] options As prepareEscalation.
 * @return The status prepareEscalation would give. OK means prepareEscalation with these same
 * unchanged values cannot refuse.
 */
Status validateEscalation(const ActionContext& output, const ActionContext& running, uint32_t operation,
                       uint64_t nowUs, uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare only the DEC-8 stop transitions; all failures leave output unchanged.
 * @param[out] output Owned context; must not overlap any input.
 * @param[in] reservation Actual continuous exclusive owner reservation, not a fresh replacement.
 * @param[in] state Fresh complete SDO STATUS/MONITORS after control; mode 1 or 3, low7,
 * alarm/release clear. Mode 3 also needs running set and predecessor 000F; it is project policy.
 * @param[in] control Actual last acknowledged 6040:00 U16 under the same reservation/history, of
 * any age. Only the read blocks expire. The word must be an allowed predecessor of kind.
 * @param[in] kind Native HALT or QUICK_STOP; no option-code interpretation.
 * @param[in] operation Nonzero identity unique while old owner work exists.
 * @param[in] nowUs Supplied nonwrapping monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable exclusive future cutoff for command and observations.
 * @param[in] options Explicit nonzero caller service/observation/freshness policies. maximumAgeUs
 * must cover five fields at the queue plus response budgets, or no cycle could ever qualify. A mode 3
 * QUICK_STOP whose maximumAgeUs covers seven fields observes standstill from its first cycle
 * (ActionContext::standstillObserving); with five it ends outside its scope on a release.
 * service.identityWidths must stay DECLARED_U16 and service.countWidths DECLARED_U8.
 * @return Local validation status only; emits no I/O and establishes no firmware qualification.
 */
Status prepareStop(ActionContext& output, const AxisReservation& reservation,
                   const ReadContext& state, const CANOPEN::SdoCompletion& control, StopKind kind,
                   uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                   const ActionOptions& options) noexcept;
/** Prepare QUICK_STOP from a running HALT, using its own acknowledgement and latest cycle.
 * No separate read can run during a stop, so this is that path's fresh state. The
 * word is 010F, a project decision, so predecessorDocumented is false. The observed mode must
 * still match the HALT's frozen mode; mode 3 additionally requires running set.
 * @param[out] output Owned context; must not overlap any input.
 * @param[in] running The running or just finished HALT.
 * @param[in] operation Nonzero identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options As prepareStop.
 * @return Local validation. An unacknowledged HALT, or no fresh complete cycle after
 * its acknowledgement, rejects, and the application takes the best-effort path.
 */
Status prepareEscalation(ActionContext& output, const ActionContext& running, uint32_t operation,
                         uint64_t nowUs, uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare one CL86-C/OL86-C alarm-clear attempt; emits no enable, mode or motion command, and no
 * controlword other than CONTROLWORD_BIT7's two reset words.
 * Requires complete fresh SDO ERRORS/ERROR_HISTORY/MONITORS/STATUS acquired under
 * reservation: 2100 alarm set and raw 603F=FF05, in one of two modes. Mode 1: 2100 running clear.
 * Mode 3: 2100 released set, and running either clear or, as the latched release a moving stop or
 * the FF05 run left (2100=001D), set with a fresh FEEDBACK block in the same read reporting 606C
 * raw zero; without that block a running report is refused. The FF05 code narrows the attempt; it
 * does not identify the reported alarm, so incident.codeLink is UNRESOLVED (D-26). EN86/ZH86 p20
 * name no mode for the resettable class, so both modes are project policy: mode 1 is where the
 * clear was designed, and mode 3 was added after a mode 3 run raised FF05 on node 1 on 2026-11-13,
 * which the mode 1 rule refused and which the bench's mode 1 route could not leave while 1001 was
 * set. Mode 3 needs the release because the drive keeps its 60FF target and nothing documents what
 * it does after a clear.
 * Writes the method's two values, the second only after the first one's acknowledgement:
 * OBJECT_2301 writes 2301 U16=1, then 0; CONTROLWORD_BIT7 writes 6040 U16=0x0080, then 0x0000
 * (AlarmClearMethod gives the sources). The second returns to the documented ineffective value:
 * for 2301 as library policy, since no source requires an edge, a rearm or a pulse, and no arming 0
 * precedes the 1 (D-27); for 6040 as p15's manual return. Completion requires a fresh cycle after
 * the second write's acknowledgement in the admission's mode (incident.modeRaw) with 2100 alarm
 * clear, then one ERRORS read; any report outside the mode's rule below ends the attempt
 * OBSERVATION_SCOPE with that report kept, and a report that is in scope but not complete repolls.
 * Mode 1: every cycle must read running clear; 6041 is kept raw and decides nothing.
 * Mode 3: every cycle must read 6041 low bits 000, since any switched-on pattern (bits 0..2 001, 011
 * or 111, as in 8031, 8033, 8027 or 8037) may be a drive acting on its retained 60FF target. While
 * 2100 still reports the alarm, the cycle must also read released, and running only after a
 * latched admission. Once 2100 reports the alarm clear, running must read clear, the release may
 * read set or clear, and completion also needs 6041 bits 3 and 7 clear, the alarm bit of ZH86 p12
 * and of EN86 p12 and OP86 p10; either set keeps the attempt polling. That rule is measured: on
 * node 1 (firmware 1000=00040192, 1009=000c, 100A=0020) on 2026-11-13, trial 5323, the 2301 method
 * admitted from 2100=001D, 6041=C018, 606C 0 and, in the first cycle after the 0's acknowledgement,
 * read 2100=0001 (alarm, running and released clear), 2102=0000, 6041=8010 and 6061=3; the earlier
 * rule, released in every cycle, ended that attempt OBSERVATION_SCOPE. What 8010 beside 0001 means
 * for torque is not established (the power-up holding report is 8037/0001). That report completes
 * a 2301 attempt, yet on the bench only a later 6040 0x0080 released the drive from it, a word the
 * 2301 method never writes. The controlword method's 11 clears on both specimens, one firmware,
 * ended with 6041=8050 and 2100=0011, which this rule completes (AlarmClearMethod). Cycles read no
 * FEEDBACK. ERRORS and ERROR_HISTORY may consume data, so polling reads MONITORS and STATUS only and
 * never rereads the history. 603F may keep its old raw value; the final read keeps it raw. A
 * persisting alarm repolls until the deadline and never repeats a write. maximumAgeUs must cover
 * five fields at the queue plus response budgets, service.identityWidths must stay DECLARED_U16 and
 * service.countWidths DECLARED_U8.
 * @param[out] output Owned context, unchanged on failure and disjoint from inputs.
 * @param[in] reservation Continuous actual owner exclusion, CL86-C/OL86-C.
 * @param[in] state Complete fresh prerequisite, FEEDBACK too for a mode 3 latched release; its raw
 * values other than FEEDBACK stay in incident.
 * @param[in] operation Nonzero live identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options Nonzero caller policies.
 * @param[in] method The reset to write, kept in clearMethod. Another value returns INVALID_CONFIG
 * with ActionError::ARGUMENT. The default, CONTROLWORD_BIT7 since 2026-11-13, is the method that
 * cleared FF05 and left the drive released on the bench (AlarmClearMethod gives the counts and
 * scope); before then it was OBJECT_2301, which a caller now names to keep the 2301 writes.
 * @return Local validation only. Caller/owner separately retains EMCY/error history;
 * this action never erases failed operations, retries, enables or resumes work. An observed
 * alarm-clear report is not proof that the cause is gone or permission to close an incident.
 */
Status prepareClearAlarm(ActionContext& output, const AxisReservation& reservation,
                         const ReadContext& state, uint32_t operation, uint64_t nowUs,
                         uint64_t deadlineUs, const ActionOptions& options,
                         AlarmClearMethod method = AlarmClearMethod::CONTROLWORD_BIT7) noexcept;
/** Prepare an explicit zero-only device counter clear; nonzero is UNSUPPORTED right after the
 * overlap check, before any state, axis or budget check.
 * Writes 2302 U16 values 0, 1, 0, each after the previous acknowledgement. The leading 0 arms the
 * documented 0 to 1 change and the last 0 is the manual return (EN86/ZH86 p15). At every possible
 * transmission use the axis-aware notice below: the first possible 1 withdraws origins, soft-limit
 * qualification, the reference witness and so every prepared target and cached observation,
 * exactly once, before the 1 can leave. Configured limit values stay. Post-ACK reads use that new
 * configuration epoch and observe signed raw 6064 zero only; no host origin, physical home,
 * arbitrary setter, counter mapping, standstill or zero-velocity claim is established, and an
 * already-zero baseline shows no change at all.
 * @param[out] output Owned atomic context, disjoint from all inputs.
 * @param[in] reservation Actual continuous CL86-C/OL86-C exclusion.
 * @param[in] state Complete fresh SDO MONITORS, STATUS and FEEDBACK under reservation: mode 1,
 * 2100 running and alarm clear, 606C raw zero. 6064 is kept raw in baseline. This is the library's
 * conservative stationarity evidence, not an observed shaft: a live test also needs that.
 * @param[in] nativePosition Native device request; only exactly zero supported.
 * @param[in] axis Valid matching axis with room in both configuration/confidence epochs.
 * @param[in] operation Nonzero live identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Exclusive future cutoff. @param[in] options Explicit caller policies;
 * maximumAgeUs must cover seven fields at the queue plus response budgets,
 * service.identityWidths must stay DECLARED_U16 and service.countWidths DECLARED_U8.
 * @return Local validation. All three writes keep the original binding. After the last
 * acknowledgement, release old-epoch results and call CanBusOwner::advanceConfiguration before
 * admitting the new-epoch observation uploads. An uncertain write leaves its route unresolved:
 * no epoch advance, result release, new reservation or MCU restart qualifies it.
 */
Status prepareSetDevicePosition(ActionContext& output, const AxisReservation& reservation,
                                const ReadContext& state, int64_t nativePosition,
                                const AxisConfig& axis, uint32_t operation, uint64_t nowUs,
                                uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Report what prepareEnable would refuse, changing nothing. For a caller that must know before it
 * moves other state, such as an owner operation.
 * @param[in] output The context prepareEnable would write; read only for its overlap checks.
 * @param[in] reservation As prepareEnable. @param[in] state As prepareEnable.
 * @param[in] identity As prepareEnable.
 * @param[in] evidence As prepareEnable. @param[in] operation As prepareEnable.
 * @param[in] nowUs As prepareEnable. @param[in] deadlineUs As prepareEnable.
 * @param[in] options As prepareEnable.
 * @return The status prepareEnable would give. OK means prepareEnable with these same unchanged
 * values cannot refuse.
 */
Status validateEnable(const ActionContext& output, const AxisReservation& reservation,
                      const ReadContext& state, const IdentityObservation& identity,
                      const EnableEvidence& evidence, uint32_t operation, uint64_t nowUs,
                      uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare the documented position-mode enable sequence: regular under EnableGate::MEASURED, an
 * attended trial under EnableGate::ATTENDED_EXPERIMENT. Neither qualifies another firmware or model.
 * Writes nothing itself. nextAction first yields one upload each of 607A and 60FF, as the project's
 * accepted action prerequisite, not a poll; their effects stay unknown. Under ATTENDED_EXPERIMENT their
 * bytes must equal the reviewed values, or the action fails before any 6040 word: this is the decision
 * for a target of unknown origin. Under MEASURED they are kept and not compared. It then yields
 * 6040 U16 words 6, 7, 15, one at a time. After each acknowledgement
 * it reads 2100..2102, 6041, 6061, 6064 and 606C until 6041 bits 0..2 read 001, 011 or 111, then
 * holds the next word until options.commandIntervalUs after that acknowledgement. Those patterns
 * come from applying the correspondence table of EN86/ZH86 p12 (6040=6 gives 001, 7 gives 011, 15
 * gives 111) as an expectation; no source documents a report after a write or its delay. Success
 * also needs 2100 released clear. Each cycle ends the sequence if mode leaves 1, a 2100 bit other
 * than released changes, a 6041 bit other than 0..2 and 10 (STATUS_EN86_ZERO_SPEED) changes, 6064
 * changes or 606C is nonzero. 2100
 * decides alarm and running because EN86 p17 and ZH86 p18 agree on it; disputed 6041 bits decide
 * nothing and are only compared. Never zeroes a target, changes mode, clears a fault, replays a
 * word or issues a stop. Fresh specimen reads show low bits 7 and released clear with no enable in
 * any retained record, so only the 1, 3, 7 progression distinguishes this sequence; even success is
 * a drive report, not torque.
 * @param[out] output Owned context, unchanged on failure, disjoint from all inputs.
 * @param[in] reservation Continuous actual owner reservation, CL86-C/OL86-C.
 * @param[in] state Complete fresh SDO read of ERRORS, MONITORS, STATUS and FEEDBACK acquired under
 * reservation, with 1001, 603F and 606C zero, in mode 1 (mode 6 only as below). Mode 0 and 3 are
 * refused. Under MEASURED the report alone chooses the start: the power-up report (EnableStart::
 * POWER_UP: 6041 low bits 111; 2100 in place with released, running, alarm, both soft limits and every
 * undocumented bit clear) or the released standstill (EnableStart::RELEASED: 2100 released, in place
 * and running either way, the other bits clear; 6041 bits 0..6 0x50 or 0x31). Any other report, such
 * as the switched-on 8033/0001, is refused with ActionError::STATE: it is outside the measured starts.
 * Under ATTENDED_EXPERIMENT the caller's start decides: NO_MOTION_WRITE_RECORDED needs 2100
 * position-complete set, running, alarm, both soft limits and every undocumented bit clear, 2100
 * released either way, and 6041 equal to the reviewed word; low bits 111 with released clear are
 * refused unless evidence.powerUpShaft records an attended observation. RELEASED needs the released
 * standstill as above. The released sequence differs as EnableStart lists: the latch must clear by
 * word 7, word 15 waits for word 7's report to hold still, and 6064 or 606C changing after word 15
 * fails with a status message telling the caller to stop the drive.
 * @param[in] identity Current identity read of this boot (checkIdentity), under either gate, compared
 * with measuredIdentity() by the prompt 57 identity policy. No observation of this target, node
 * generation and binding, or product fields (1000, 1018:01, 1018:02, 2000) that differ, return
 * UNSUPPORTED with ActionError::TARGET before any TX; no gate or option overrides it. Firmware fields (1009, 100A, 1018:03, 2001) that differ admit the enable and
 * set output.enable.identity.firmwareUnqualified, with both tuples kept. The documented reference is
 * reported only: its 2000 default (1) is not what either specimen reads (0x0400).
 * @param[in] evidence Copied. A closed gate returns UNSUPPORTED. MEASURED consults only modeRaw, which
 * must be 1 (otherwise UNSUPPORTED), operation and start: UNKNOWN admits either measured start, RELEASED
 * or POWER_UP (NO_MOTION_WRITE_RECORDED counts as POWER_UP) requires that class. ATTENDED_EXPERIMENT
 * needs the trial, reservation, operation, expiry, start statement, shaft observation and exact
 * reviewed values, and refuses POWER_UP. Anything missing or mismatched returns INVALID_CONFIG, and a
 * start report outside the start's rule returns it with ActionError::STATE.
 * @param[in] operation Nonzero identity unique while old owner work exists, equal to
 * evidence.operation.
 * @param[in] nowUs Nondecreasing monotonic microseconds, zero valid; under ATTENDED_EXPERIMENT before
 * evidence.expiresUs.
 * @param[in] deadlineUs Immutable exclusive cutoff, more than three intervals plus
 * ENABLE_SERVICE_STEPS queue-and-response budgets ahead. That guarantees only that a drive which
 * answers every service within its budgets and shows each report in the first cycle can finish:
 * it does not guarantee completion. A later report, a fault, a lost reply or a withdrawal can still
 * end the sequence after word 6 or 7, leaving the drive in a partial state whose effects are
 * unknown.
 * @param[in] options Nonzero budgets; commandIntervalUs at least ENABLE_WORD_INTERVAL_US;
 * maximumAgeUs covering nine fields at the queue plus response budgets; DECLARED_U16 identity widths.
 * @return Local validation only. No I/O. Never establishes firmware qualification or a safe enable.
 */
Status prepareEnable(ActionContext& output, const AxisReservation& reservation,
                     const ReadContext& state, const IdentityObservation& identity,
                     const EnableEvidence& evidence, uint32_t operation, uint64_t nowUs,
                     uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare one documented operating-mode selection on CL86-C/OL86-C: a single 6060:00 download of
 * modeRaw, settled only when a later fresh 6061 reads modeRaw. The request is the one byte
 * `2F 60 60 00 <modeRaw> 00 00 00`: the manuals declare 6060 INTEGER8 and the EDS U8, and 1, 3 and 6
 * are the same byte either way. It never reads 6060, which the EDS (LC_StepDriverCAN_V1.1.eds) declares
 * write-only while the manual tables list it RW (D04); never writes 6061, the drive's mode report
 * (EN86's "set 6061" is a known mistranslation of monitor); and never writes 6040, enables, clears,
 * stops or resends anything.
 *
 * Admission needs a complete fresh SDO read of ERRORS, MONITORS, STATUS and FEEDBACK acquired under
 * reservation: 1001 and 603F zero, 2100 alarm clear, 606C zero, and one of the two starts of
 * ModeSelectStart:
 * - NOT_ENABLED: 2100 released set and 6041 bits 1 and 2 clear, any start mode and any requested mode.
 *   2100 running may be set beside the release only as the latch a moving quick stop leaves (2100=0014 or
 *   0015 with 6041=4050 or C050, which stays for minutes); 606C zero in the same read admits it. When
 *   6061 already reads modeRaw, the answer is already selected: the context ends SUCCEEDED and OBSERVED
 *   at preparation, execution NOT_TRANSMITTED and modeSelect.alreadySelected set, and nothing is ever
 *   yielded or sent.
 * - POWER_UP: 6061 0, 6041 exactly 8037 and 2100 exactly 0001, to mode 1 only: all 23 measured writes
 *   from it were 6060=1.
 * Every other report is refused before TX. That includes an enabled drive (6041 low bits 111 with 2100
 * released clear in any mode but the power-up report): no source documents a mode change while enabled
 * (Q11), and only the power-up writes measured one. It also includes the switched-on reports 8033 and
 * 8010 beside 2100=0001 (backlog S21 and the 2301 clear), from which no mode write was measured.
 *
 * From the acknowledgement on it reads MONITORS, STATUS and FEEDBACK in cycles, each after the previous
 * one by observationIntervalUs, until a cycle in scope reports 6061 equal to modeRaw. A cycle that starts
 * after the acknowledgement is the only settlement evidence. A cycle is in scope when 6061 reads the
 * start mode or modeRaw, 2100 alarm stays clear, 2100 released and the class of 6041 bits 0..2 repeat the admission's
 * (000 or 001 after NOT_ENABLED, 111 after POWER_UP), 2100 running is set only where the admission read
 * it, 606C reads zero and 6064 repeats the admission value. Other 6041 bits decide nothing: bit 10 went
 * from 8037 to 8437 with the power-up mode change. A cycle still reporting the start mode repolls until
 * the deadline; any other report ends the action OBSERVATION_SCOPE with that cycle kept. Measured: about
 * 130 writes on two CL86-C specimens, one firmware (1000=00040192, 1009=000c, 100A=0020), all
 * acknowledged and read back: 23 from the power-up report, the others 1 to 3 and 3 to 1 released
 * (trials 917, 2101 and 2221 among them, and the night soaks). No mode 6 write and no write from a start
 * mode other than 0, 1 or 3 has run. Success is the drive's mode report, not a qualification of that
 * mode's motion.
 * @param[out] output Owned context, unchanged on failure, disjoint from all inputs.
 * @param[in] reservation Continuous actual owner reservation, CL86-C/OL86-C.
 * @param[in] state Complete fresh prerequisite read as above, borrowed for this call. Its STATUS,
 * MONITORS and FEEDBACK seed the observations; modeSelect keeps the raw start.
 * @param[in] modeRaw The 6060 code: 1 position, 3 velocity or 6 homing (EN86/ZH86 p11). Any other
 * value returns UNSUPPORTED with ActionError::ARGUMENT right after the overlap check, before any state,
 * reservation or budget check: undocumented codes belong to the D17 probe, not to this operation.
 * @param[in] operation Nonzero identity unique while old owner work exists.
 * @param[in] nowUs Nondecreasing monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable exclusive future cutoff of the write and every observation.
 * @param[in] options Nonzero budgets and observationIntervalUs; maximumAgeUs covering seven fields at the
 * queue plus response budgets; service.identityWidths DECLARED_U16 and service.countWidths DECLARED_U8.
 * @return Local validation only, no I/O: OVERLAP, then UNSUPPORTED for the code, then ARGUMENT for the
 * reservation, identity, time or budgets, then STATE for the read or the start report, each with output
 * unchanged; OK for a prepared write or an already selected mode. The prepared context's retryClass is
 * NOT_REPEATABLE. Later, execution keeps the 6060
 * acknowledgement apart from completion: an acknowledged write whose cycles never report modeRaw ends
 * without completion and uncertain, a lost reply leaves execution UNKNOWN, an abort REJECTED, and
 * nothing is resent. A new attempt is a new operation, prepared from a new fresh read.
 */
Status prepareModeSelect(ActionContext& output, const AxisReservation& reservation,
                         const ReadContext& state, uint8_t modeRaw, uint32_t operation, uint64_t nowUs,
                         uint64_t deadlineUs, const ActionOptions& options) noexcept;
/** Prepare a release (free the shaft) on CL86-C/OL86-C by the release table (ReleaseRow): one 6040 U16
 * word, the row's, or an explicit experiment's, settled only when a later fresh cycle reports the row's
 * 6041 low bits with 2100 released. 606C is recorded with that cycle and a nonzero value is reported
 * (ReleaseRecord::settledVelocityNonzero); a release never claims standstill, holding torque or a
 * reference. It never writes another object, enables, stops, clears or resends: a refused, lost or
 * unacknowledged word is not repeated, and a new attempt is a new operation from a new fresh read.
 *
 * Admission needs a complete fresh SDO read of ERRORS, MONITORS, STATUS and FEEDBACK under reservation,
 * classified by classifyRelease. Row 4 (already released) ends SUCCEEDED and OBSERVED at preparation,
 * execution NOT_TRANSMITTED, the running bit and 606C reported in release.decision, nothing ever sent.
 * Row 5 is refused with ActionError::STATE and the refusal's reason, unless the experiment overrides it.
 * From the acknowledgement on it reads MONITORS, STATUS and FEEDBACK in cycles, each after the previous
 * one by observationIntervalUs; a cycle is in scope while 6061 repeats the admission's mode, 2100 alarm
 * stays clear and 2100 running is set only where the admission read it. A cycle in scope that does not
 * yet report the settlement repolls until the deadline; any other report ends it OBSERVATION_SCOPE.
 * @param[out] output Owned context, unchanged on failure, disjoint from all inputs.
 * @param[in] reservation Continuous actual owner reservation, CL86-C/OL86-C.
 * @param[in] state Complete fresh prerequisite read as above, borrowed for this call.
 * @param[in] loop Optional fresh CONTROL observation (getControl) whose 2500 decides row 2; null leaves
 * row 2's report refused as LOOP. When given it must be current for the reservation's target, begun
 * under the reservation and within options.maximumAgeUs, or STATE is returned.
 * @param[in] experiment Off by default; see ReleaseExperiment. A word other than 0x0000 or 0x0006 is
 * refused with ActionError::ARGUMENT before any state check.
 * @param[in] operation Nonzero identity unique while old owner work exists.
 * @param[in] nowUs Nondecreasing monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable exclusive future cutoff of the word and every observation.
 * @param[in] options Nonzero budgets and observationIntervalUs; maximumAgeUs covering seven fields at the
 * queue plus response budgets; service.identityWidths DECLARED_U16 and service.countWidths DECLARED_U8.
 * @return Local validation only, no I/O: OVERLAP, ARGUMENT, STATE, each with output unchanged; OK for a
 * prepared word or an already released report. retryClass is NOT_REPEATABLE. Later, execution keeps the
 * word's acknowledgement apart from completion: a lost reply leaves it UNKNOWN, an abort REJECTED.
 */
Status prepareRelease(ActionContext& output, const AxisReservation& reservation, const ReadContext& state,
                      const ControlObservation* loop, const ReleaseExperiment& experiment,
                      uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                      const ActionOptions& options) noexcept;
/** Record a local withdrawal while this action's admitted work is still in flight, so that its
 * terminal is consumed as evidence and cannot complete the action. Sends nothing and stops nothing.
 * @param[in,out] context Active context with no earlier withdrawal; unchanged on refusal.
 * @param[in] live Actual reservation; INVALIDATED alone permits a changed or withdrawn reservation.
 * @param[in] current Actual full target and epochs, checked except for INVALIDATED.
 * @param[in] reason CANCELLED, DRIVE_FAULT or INVALIDATED. At or after the cutoff it is DEADLINE.
 * @param[in] nowUs Nondecreasing supplied time.
 * @return Local status. Afterwards nextAction yields nothing, and the next accepted terminal ends
 * the action with this reason, keeping that terminal in commandHistory or the observation evidence.
 * The local advanceAction overload still ends it at once once the work has been retired.
 */
Status noteActionWithdrawal(ActionContext& context, const AxisReservation& live,
                            const ReadTarget& current, ActionOutcome reason, uint64_t nowUs) noexcept;
/** Yield copied work without mutation, I/O or automatic replay.
 * @param[in] context Prepared action. @param[in] live Actual continuous owner reservation.
 * @param[in] current Actual current full target/epochs. @param[in] nowUs Nondecreasing caller time.
 * @param[out] output Unchanged on rejection; no overlap with inputs. ready=false during wait/terminal,
 * and for an observation read once one exchange's queue and response budgets no longer fit before
 * the deadline, so none can be cut there; commands are not held back.
 * @return Local status; active calls at/after cutoff reject until explicitly advanced to DEADLINE.
 */
Status nextAction(const ActionContext& context, const AxisReservation& live,
                  const ReadTarget& current, uint64_t nowUs, PreparedAction& output) noexcept;
/** Record possible stop/alarm/enable/mode command transmission; never sends or modifies a binding.
 * Counter actions reject this overload to prevent bypassing coordinate invalidation.
 * @param[in,out] context Pending download, exactly one notice per step. Uploads use owner terminals.
 * @param[in] live Actual reservation. @param[in] current Actual target/epochs.
 * @param[in] nowUs Nondecreasing time before cutoff and not before eligibility.
 * @return Local status; rejected/overlapping inputs leave context unchanged.
 */
Status noteActionTransmission(ActionContext& context, const AxisReservation& live,
                              const ReadTarget& current, uint64_t nowUs) noexcept;
/** Axis-aware counter notice; required for every counter-action download.
 * First possible U16=1 trigger atomically invalidates coordinates/origins/limits,
 * advances both axis epochs and current's configuration, and records uncertainty.
 * Initial/final zero only note possible TX and never withdraw again. No rollback follows
 * owner refusal or cancellation after this conservative boundary. A trigger refused or
 * cancelled before this notice leaves coordinatesInvalidated false and every epoch as it
 * was. Does no I/O or persistence.
 * @param[in,out] context Active counter context. @param[in] live Frozen live reservation.
 * @param[in,out] current Actual current target, old then exactly one advanced configuration.
 * @param[in,out] axis Actual frozen matching axis interpretation.
 * @param[in,out] reference Position witness to withdraw, no replacement synthesized.
 * @param[in] nowUs Monotonic time before cutoff. @return Atomic status on rejection;
 * all mutable outputs and borrowed reservation must be mutually disjoint.
 */
Status noteActionTransmission(ActionContext& context, const AxisReservation& live,
                              ReadTarget& current, AxisConfig& axis, AxisReference& reference,
                              uint64_t nowUs) noexcept;
/** Consume actual matching retained owner terminal; invalid evidence is atomic.
 * @param[in,out] context Active action. @param[in] live Actual continuous reservation.
 * @param[in] current Actual full target/epochs. @param[in] evidence Borrowed terminal, no overlap.
 * @param[in] nowUs Supplied processing time at least completion and previous mutation.
 * @return Local call status. Accepted service failures settle operation; deadline wins at equality.
 * Matching owner evidence supersedes a conservative possible-TX notice. It cannot prove wire
 * freshness: channel ambiguity/quarantine must already be handled by the sole owner.
 */
Status advanceAction(ActionContext& context, const AxisReservation& live,
                     const ReadTarget& current, const CANOPEN::SdoCompletion& evidence,
                     uint64_t nowUs) noexcept;
/** Local termination after separately retiring any admitted work; never another stop/replay.
 * @param[in,out] context Active context, retained evidence preserved.
 * @param[in] live Actual reservation; INVALIDATED alone permits a changed/withdrawn reservation.
 * @param[in] current Actual full target/epochs, checked except for INVALIDATED.
 * @param[in] reason CANCELLED, DEADLINE, INVALIDATED or independently checked DRIVE_FAULT.
 * @param[in] nowUs Nondecreasing supplied time; inclusive deadline wins over local reason.
 * @return Local status. Earlier terminal is immutable; command ACK survives observer cancellation.
 */
Status advanceAction(ActionContext& context, const AxisReservation& live,
                     const ReadTarget& current, ActionOutcome reason, uint64_t nowUs) noexcept;
/** Prepare a best-effort stop; no state evidence, no history and no traffic.
 * @param[out] output Owned context; unchanged on failure; must not overlap any input.
 * @param[in] target Actual current route and node epoch.
 * @param[in] operation Nonzero identity. @param[in] nowUs Monotonic microseconds.
 * @param[in] deadlineUs Immutable exclusive future cutoff; a recovery never extends it.
 * @param[in] service Nonzero queue and response budgets.
 * @return Local validation only.
 */
Status prepareBestEffortStop(BestEffortStop& output, const ReadTarget& target, uint32_t operation,
                             uint64_t nowUs, uint64_t deadlineUs,
                             const ReadOptions& service) noexcept;
/** Yield the exact stop for CanBusOwner::admitBestEffortStop, without mutation or I/O.
 * @param[in] context Active stop not yet admitted. @param[in] current Actual current target.
 * @param[in] nowUs Nondecreasing time. @param[out] output Unchanged on rejection.
 * @return Local status; a changed target or reached deadline rejects.
 */
Status nextBestEffortStop(const BestEffortStop& context, const ReadTarget& current, uint64_t nowUs,
                          PreparedAction& output) noexcept;
/** Record that the owner accepted the stop; from here only its terminal can settle it.
 * Admission conservatively sets execution UNKNOWN and effectsPossible even while the frame
 * remains queued locally. A terminal proving it unsent refines those to NOT_TRANSMITTED and false.
 * @param[in,out] context Active stop not yet admitted. @param[in] nowUs Nondecreasing time.
 * @return Local status; rejection leaves context unchanged.
 */
Status noteBestEffortAdmission(BestEffortStop& context, uint64_t nowUs) noexcept;
/** Consume the owner terminal of an admitted stop. The target may have changed since.
 * SUCCESS, a server abort or UNSUPPORTED cannot be this path's terminal and is rejected as
 * correlation, so a matching-looking reply cannot make this stop succeed.
 * @param[in,out] context Admitted stop. @param[in] evidence Borrowed terminal, no overlap.
 * @param[in] nowUs Processing time at least the completion and previous mutation.
 * @return Local call status. The accepted terminal sets outcome and execution.
 */
Status advanceBestEffortStop(BestEffortStop& context, const CANOPEN::SdoCompletion& evidence,
                             uint64_t nowUs) noexcept;
/** End a stop that was never admitted, so nothing can have left.
 * @param[in,out] context Active stop. @param[in] reason CANCELLED, DEADLINE, INVALIDATED or TRANSPORT_ERROR.
 * @param[in] nowUs Nondecreasing time; an inclusive deadline wins over the local reason.
 * @return Local status; an admitted stop must end through its terminal.
 */
Status advanceBestEffortStop(BestEffortStop& context, ActionOutcome reason,
                             uint64_t nowUs) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
