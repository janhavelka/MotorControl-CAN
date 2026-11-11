/** @file BenchRoute.h
 * @brief Named finite bench routes for attended trials, through the sole owner and AxisSession.
 * SPDX-License-Identifier: MIT
 *
 * Application only: no SDK headers, heap, clock, retry or recovery. One cooperative caller services
 * the firmware first, then this object, on the owner task. Every frame goes through an AxisSession
 * opened with ProbeApp::openAxis on the ordinary owner: typed reads with their own read effect,
 * one-use experiment grants for every unqualified upload, and the session's own enable and stop.
 * There is no raw sender, no object selector and no command language: each route is a fixed table.
 *
 * Routes, one at a time, each with a new trial identity that rises for the life of this object:
 * - READ_ACTIONS, packet 15 P1. STATE(1001, 603F, 2100..2102, 6041, 6061, 6064, 606C),
 *   MOTION_PARAMETERS(6081, 6083, 6084), granted single uploads 1017, 607A, 60FF, then the
 *   accepted image (2100..2102, 6041, 6061, 6064, 606C) again. No writes.
 * - READ_IO, packet 13 P1. Image, IO_INPUT_CONFIGURATION(2400:00..08), IO_OUTPUT_CONFIGURATION
 *   (2400:0C..0F), IO_INPUT_FILTERS(2401..2407), IO_OUTPUT_COMMAND(60FE:00..02), image. 2400:0B is
 *   never read. No writes. 60FE:00 is read in two bytes (CountWidths::MEASURED_U16), as both
 *   specimens answered it; every other group keeps its declared widths.
 * - READ_TUNING, packet 14 T0. Image, the four tuning groups 2502..2520, image. No writes.
 * - READ_HISTORY, packet 4 and packet 22 step 2. STATE(1001, 603F, 2100..2102, 6041, 6061, 6064, 606C),
 *   STATE(ERROR_HISTORY) alone: 1003:00, then only the entries its count names, at most four, then
 *   STATE(ERRORS) again, so 1001 and 603F are on record before and after the first 1003 read. Every
 *   1003 upload is unqualified and takes its own one-use grant. No writes; the values are kept raw
 *   and never interpreted. Last in RouteKind, so earlier route numbers and records are unchanged.
 * - MODE_ONE, packet 15 P2. The READ_ACTIONS reads, a prerequisite check, one 6060:00=1 through
 *   AxisSession::admitPrepared, then the stated number of fresh cycles. Never enables. Two starts,
 *   chosen by the first read's mode, both in a session with no write or controlword in this boot. Mode 0,
 *   the power-up baseline: the check as packet 15 P2 states it, then the stated cycles, recorded. Mode 3,
 *   the released standstill a recorded stop left with the drive kept powered (2026-11-10): a second
 *   fresh full read, and both must report 2100 bit 4 released, alarm clear, 6041 bits 0..6 the release
 *   pattern 0x50, mode 3, 606C 0, 1001 and 603F 0 and the same 6064, with 2100 running admitted latched;
 *   then the same one 6060=1, and cycles until one reads 6061=1, at most the stated number, each still
 *   released with low bits 000, 606C 0 and the check's 6064, running only if the check read it latched.
 *   No 6040 in either. The line is unchanged.
 * - ENABLE, packet 15 P3. A fresh ERRORS/MONITORS/STATUS/FEEDBACK read, then
 *   AxisSession::requestEnable with the host's trial, start record, reviewed 6041, 607A and 60FF and the
 *   attendant's shaft observation. The firmware never supplies an observation itself. Start 2,
 *   EnableStart::RELEASED_BY_RECORDED_STOP, carries shaft 1 (FREE): the basis is the released start
 *   report, read from the drive at the user's instruction of 2026-11-10, never felt, and the library
 *   does not consult it for a released start. The library admits the running latch a moving quick stop
 *   leaves, which must clear by word 7, holds word 15 until word 7's report reads still, and fails on a
 *   6064 or 606C change after word 15. When a start 2 enable fails by itself once its word 15 may have
 *   reached the drive (TRIAL_ENABLE_STEP word=15 history=1 possible=1), the route asks the session for
 *   one QUICK_STOP at once: deadline and best-effort deadline 1500 ms, cycles every 500 ms (record
 *   2108's planned stop budget), cut to the cutoff; TRIAL_STOP and TRIAL_BEST_EFFORT show it. After an
 *   acknowledged 000F it passes the enable's last cycle, so the session sequences 000B on a report with
 *   low bits 111, takes the DEC-2 000B on a running one and refuses on an idle one outside the stop's
 *   states; without that acknowledgement it passes no report, and the session takes the DEC-2 000B.
 *   Every start 2 plan reserves that stop's terminals and time. The stop is a reaction, not a planned
 *   trial, so it adds no motion weight. A failure before word 15 (the latch still reported at word 7)
 *   sends nothing more: the drive is then switched on.
 * - STOP, packet 15 P4 and packet 16. A fresh MONITORS/STATUS read, then AxisSession::requestStop
 *   with the host's kind and policy. The path taken and every context it wrote are reported.
 * - POSITION, packet 19. An application-local isolated experiment, not a move engine: a fresh read,
 *   a prerequisite check against the host's reviewed start position and the session's acknowledged
 *   000F, the earlier 6081/6083/6084/607A for the record, then 607A, 6081, 6083, 6084 and one
 *   6040=001F trigger as host-reviewed native values, each after the previous acknowledgement, then
 *   fresh cycles and an optional named stop. MotionPreview stays an offline review artifact.
 * - REARM, packet 19. The post-terminal 000F as its own visible step, project policy until the mode 1
 *   falling-edge behavior is measured. The host names the predecessor it expects, and the route is
 *   admitted only while the session's latest controlword is exactly that acknowledged word on that
 *   node: 1 the 001F trigger of a finished POSITION record, 2 the 010F of that record's planned HALT
 *   (does clearing a halt resume the halted target?), 3 the 005F trigger of a finished RELATIVE
 *   record. A fresh full read and a check, then exactly one 6040=000F through the session, each like
 *   POSITION's own writes, then the stated number of fresh cycles, recorded and never judged as the
 *   success of anything physical.
 * - MOVE, packet 20 and packet 22 step 4. A fresh identity read and full read, a check like
 *   POSITION's against the host's reviewed start position, then AxisSession::requestMove with
 *   MoveEvidence (record, acceptance and completion signals, rule and firmware identity) taken from
 *   the host's line, never defaulted here. The session runs the move to its own terminal, its rearm
 *   included; an optional named stop after a stated number of move cycles goes through STOP's path
 *   with the move's last complete cycle. With a relative base on the line (1 ACTUAL, 2 COMMANDED,
 *   the host's packet 21 measurement) the target is a displacement: a second fresh full read must
 *   pass the same check, and AxisSession::requestMoveRelative gets that basis and an idle,
 *   stationary witness from the second read (ACTUAL: its 6064; COMMANDED: the host's reviewed base).
 * - RELEASED_QUICK, packet 16 step 3. Two fresh full reads that must both show 2100 bit 4 released,
 *   alarm clear, 6041 low bits 000, mode 1, 606C 0, 1001 and 603F 0, and the same 6064 whenever either
 *   shows 2100 bit 2, which can stay latched after a 000B in motion. Then one 6040=000B through
 *   AxisSession::admitPrepared under its own grant, then the stated number of fresh cycles, recorded
 *   and never judged. An application-local experiment, not a stop: requestStop refuses a released
 *   drive by design. The session's latest controlword is recorded, not required.
 * - WORD67, packet 15. RELEASED_QUICK's reads and check, then one 6040=0006 and 1..4 fresh cycles;
 *   only when the last of them shows mode 1 and 6041 low bits 001 one 6040=0007, then 1..16 fresh
 *   cycles. No other word: the drive is left switched on, with 0007 as the latest word.
 * - RELATIVE, packet 21. POSITION with the signed nonzero displacement in 607A and the 005F trigger,
 *   measuring what the drive adds the displacement to. TRIAL_BASE shows the 607A read before the
 *   writes and this object's own record of the last acknowledged 607A on that node.
 * - MOVE with an observer node, packet 22 step 6. The other node must first read stationary (running
 *   and alarm clear, 606C 0, released or not as found); while the move or its planned stop runs, its
 *   image is read at the move's interval through its own route, session and read context, never node
 *   A's, and it gets no write. Its cycles are recorded, never judged; an EMCY from it ends only the
 *   observing. Terminals carry their node, and TX traffic its channel.
 * - SETTINGS_2202 and IO_DI3, packets 12 T1/T2 and 13 P2/P3. The prior group read, a clean STATUS and
 *   MONITORS read, then one candidate through prepareSoftLimitSettings or prepareIoSettings on the
 *   session's reservation, its write through admitPrepared and the library's readback. IO_DI3 changes
 *   exactly one thing, DI3's function between 0 and 7 or its polarity bit, from the prior state its case
 *   names, and then reads 2101 and 60FD once. The write's pre-transmit notice advances the configuration
 *   epoch; the owner's reservation follows before the readback, and the session, which has no epoch
 *   change of its own for this, is closed and opened again on the new binding. Settings routes are not
 *   motion: they are attended and have their own cap, ROUTE_SETTINGS_PER_CASE per route and node.
 *   SETTINGS_2202 reads 607D:00 in two bytes (CountWidths::MEASURED_U16) in its prior read and its
 *   readback, as both specimens answered it.
 * - COUNTER_ZERO and CLEAR_ALARM. A fresh read of what AxisSession::requestCounterZero or
 *   requestClearAlarm judges, CLEAR_ALARM refused unless it reports the alarm, then the session's
 *   request; the session admits every write and read itself. Motion class.
 * - MODE_THREE, prompt 35's released mode change. Two fresh full reads that must both report the
 *   drive released and stationary (2100 bit 4 set, running and alarm clear, 6041 low bits 000 or 001,
 *   mode 0 or 1, 606C 0, 1001 and 603F 0, the same 6064), then one 6060:00=3 through
 *   AxisSession::admitPrepared under its own grant, then the stated number of fresh cycles, recorded
 *   and never judged. A drive that reports enabled is refused: no source documents changing 6060 while
 *   enabled (Q11). The session's latest word recorded at admission must be unchanged at the write, as
 *   for RELEASED_QUICK. Never enables. Motion class.
 * - VELOCITY, prompt 35. A fresh identity read and full read, a check that the drive reports released
 *   and stationary in mode 3, then AxisSession::requestVelocity under VelocityGate::ATTENDED_EXPERIMENT:
 *   the run measures its mode 3 start and stop and claims neither, so startQualified and stopQualified
 *   stay false and no line field can set them. The rest of VelocityEvidence comes only from the host's
 *   line (record, status layout, stop kind, raw identity), never defaulted here. The line's own rpm is
 *   the forward maximum, at most VELOCITY_EXPERIMENT_MAXIMUM_RPM, and the duration at most
 *   VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US, both checked by the library; reverse and a mode change are
 *   never claimed, so the drive must already read mode 3 (MODE_THREE). The session writes 60FF, 6083,
 *   6084, then 6, 7, 15, runs its cycles and asks for its own stop at the preparation-anchored due time.
 *   Every run sets the library's final-enable report window (backlog S17) to the experiment's chosen
 *   budget, ROUTE_VELOCITY_WINDOW_MS with cycles ROUTE_VELOCITY_SPACING_MS apart: while word 7's report
 *   still shows after word 15, another seven-field cycle follows, up to ROUTE_VELOCITY_WINDOW_CYCLES
 *   more, and the record keeps the first and last such cycle and the one that reported the enable
 *   (TRIAL_VELOCITY_WINDOW, TRIAL_VELOCITY_SAMPLE which=2..4) beside every cycle's RESULT lines. The
 *   window's cycles count in the plan's terminal bound, and the library adds the window to the setup
 *   time the duration must cover, so with the route budgets the duration must be at least 11470 ms.
 *   When the run ends still needing a stop with none of its own sent (velocityNeedsStop() with stop
 *   execution NOT_TRANSMITTED, as when no stop can be composed after word 15's reply was lost; backlog
 *   S16), the route sends one fallback QUICK_STOP through requestStop, best-effort 000B allowed, within
 *   the plan's stop span after the duration and the cutoff, before the record ends; a run withdrawn for
 *   a drive fault gets none. The session's VelocityContext keeps the experiment gate: a successful run
 *   qualifies nothing. Motion class; its own stop counts against STOP as well, and the fallback, which
 *   replaces a stop that never left, counts nothing more.
 * - HOME, prompt 36. A fresh identity read, IO_INPUT_CONFIGURATION (2400:00..08), a full read (2101
 *   with it) and DRIVER (2200, 2201), each as AxisSession::requestHome consumes it, then requestHome
 *   with a CLOSED HomeEvidence: no homing record exists for this fixture, so no line field can open the
 *   gate. The request is refused before any write, which is the route's whole result. Attended, with
 *   the settings routes' caps and no motion weight, because nothing can be written.
 * - VELOCITY_TARGET, ledger gate Q03: how the drive takes a 60FF value (EDS and EN86/ZH86 p17 I32
 *   against p13's U32 0..3000 rpm). Two fresh full reads that must both report the released mode 3
 *   standstill a velocity run starts from: 2100 bit 4 set, alarm clear, 6041 low bits 000, mode 3, 606C
 *   0, 1001 and 603F 0, the same 6064, with 2100 running admitted latched as the velocity admission
 *   admits it. Then one 60FF download of the line's signed value through AxisSession::admitPrepared
 *   under its own grant, `23 FF 60 00` and its four bytes in two's complement. Only once it is
 *   acknowledged, one granted 60FF upload kept raw, then one more fresh full read that must report the
 *   same standstill at the same 6064. An abort or a lost reply ends the route at that terminal with its
 *   abort code kept, and nothing more is sent. Never 6040, 6060 or another object, never an enable, no
 *   retry. The write is this session's own, so the enable refuses after it in this boot, and a session
 *   that may still have work running (velocity, home or move fence, fault block) refuses it first.
 *   Attended, with the settings routes' caps and no motion weight: the drive is released throughout.
 * - ARM_MOVE and ARM_VELOCITY, the shared console's arming (BenchConsole.h). MOVE's identity read, full
 *   read (two for a relative base) and check against the host's reviewed start position, or VELOCITY's
 *   identity read, full read and released mode 3 check, exactly as those routes acquire them, then the
 *   end: no write and no request. A successful record leaves its reads in place for armed(), with the
 *   line's evidence, until the next route begins or closeSessions closes a session. The line carries the
 *   evidence MOVE or VELOCITY carries, without the request (target, rate, ramps of a move, duration),
 *   which the console supplies, and the stop budgets of the console's later stop requests. Attended,
 *   reads only, no motion weight: each console motion it enables is counted when the console admits it
 *   (consoleMotionAllowed, countConsoleMotion).
 * - ARM_WITNESS, the console's host coordinate witness. Two fresh full reads that must both show the host's
 *   reviewed 6064, and either both pass MOVE's idle check (enabled, mode 1, low bits 111, running, alarm
 *   and released clear, 606C 0, 1001 and 603F 0) or both pass RELEASED_QUICK's released check, then the
 *   end. No 000F is required, so a host scale or origin can be set before the enable: a configuration
 *   change drops the session's latest controlword, and an enabled session cannot be enabled again.
 *
 * No route exchange starts once less than its queue and response budget remains before its operation
 * deadline or the route cutoff: the route then ends there with failure DEADLINE and nothing in flight.
 * A 000B is exempt, as a stop word.
 *
 * Every motion route (MODE_ONE, ENABLE, STOP, POSITION, REARM, MOVE, RELEASED_QUICK, WORD67, RELATIVE,
 * COUNTER_ZERO, CLEAR_ALARM, MODE_THREE, VELOCITY), every settings route, HOME and VELOCITY_TARGET needs an attended flag
 * that the host sets only after the operator typed a confirmation. At most ROUTE_MOTION_TOTAL motion
 * routes and ROUTE_MOTION_PER_CASE per route and node are admitted per boot; a planned stop inside
 * POSITION, RELATIVE or MOVE, and VELOCITY's own stop, counts as one more; a start 2 ENABLE's own stop
 * after a failure does not, so no cap can refuse it. These caps restart with the
 * MCU: scripts/s2_bench.py keeps the campaign's count across boots. Any failed terminal, lost reply, grant or admission refusal,
 * changed epoch, nonzero EMCY from the node, lost evidence or local abort ends the route and keeps
 * its evidence; nothing is retried, enabled, cleared, zeroed, restored or switched to another mode,
 * and an abort sends no stop. The owner receives before it transmits, so a nonzero EMCY also holds
 * back, in the very turn that received it, the node's enable, move or clear download that would
 * otherwise leave before the route has seen it: the transmit hook refuses it unsent, the action
 * ends DRIVE_FAULT and its session stays faultBlocked. A request already on its way always settles
 * first, and a frame already handed over settles as evidence. Lost evidence also
 * withdraws an active enable or move locally, as an abort does, because the lost span may have held
 * an EMCY or a status report: nothing is sent because of the loss, and a route with lost evidence
 * never ends NONE. A POSITION, RELATIVE or MOVE with a planned stop is refused before its first write
 * unless the stop's whole span still fits the route cutoff when it is asked for at its latest. The
 * predecessor a check judged is judged again just before each word write is admitted; a route that
 * requires none still requires the latest word it recorded at admission.
 *
 * VELOCITY is the one route whose operation has no endpoint of its own: its stop at the due time is
 * its only software bound. So an abort is refused while it is active, and lost evidence ends the route
 * EVIDENCE_LOSS without withdrawing it: withdrawing locally would suppress that stop and send nothing
 * in its place. A nonzero EMCY of the node is the drive's own fault report and takes the session's
 * checked fault path, cancelVelocity with DRIVE_FAULT, as AxisSession::preflight does at handover:
 * no further velocity word leaves and no stop follows. The attendant's independent cutoff is the
 * control for anything still turning.
 *
 * Sessions stay open between routes once a route has opened them: their history, latest controlword
 * and trial record must survive from one route to the next. While one is open it holds its axis's
 * owner reservation, so everything that takes its own reservation on that node (RawEvidence,
 * PdoExperiment, the K and H records) refuses. closeSessions is the explicit bench step that ends
 * this between phases of one MCU boot (backlog S15): it closes only idle sessions, sends nothing and
 * changes nothing on a drive. It is not a stop, release or disable. Closing drops each session's
 * latest controlword, so a later route that needs one refuses as after an MCU restart (a REARM,
 * POSITION, RELATIVE or MOVE with `control`, HOME with the session's `code=1 detail=3`). Everything
 * that refuses later work survives in the stationary session object and here, as it would not
 * across an MCU restart: the session's move fence, clear and fault blocks, needs-stop facts, motion
 * write history and enable trials, and this object's mode write record, trial identity, caps and
 * counters. The next route on that node reopens the same session object, which refuses the same way.
 *
 * Grant identities start at ROUTE_GRANT_BASE, above every other bench range. The channel grants an
 * experiment only above its last grant, in authorization and in operation identity, whoever drew
 * it (SdoClient::authorizeUploadExperiment), so each route grant also continues above the channel's
 * last grant (SdoClient::lastExperiment), and begin raises the route's sessions above its last
 * granted operation: after K or H the session's own numbering was below theirs, and the homing
 * route's 2400 read was refused `why=grant` (trials 903 to 906). A later DeviceEvidence grant on
 * the same channel is still refused, which fails closed.
 */
#pragma once
#include "FirmwareProbe.h"
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

namespace MotorControlCANExample {
/** Fixed route selection; not an object selector. */
enum class RouteKind : uint8_t {
    NONE,         ///< Unparsed or empty plan; refused.
    READ_ACTIONS, ///< Packet 15 P1 reads; `TA`.
    READ_IO,      ///< Packet 13 P1 reads; `TI`.
    READ_TUNING,  ///< Packet 14 T0 reads; `TU`.
    MODE_ONE,     ///< Packet 15 P2, one 6060:00=1; `TM`.
    ENABLE,       ///< Packet 15 P3, AxisSession::requestEnable; `TE`.
    STOP,         ///< Packet 15 P4 and packet 16, AxisSession::requestStop; `TS`.
    POSITION,     ///< Packet 19 isolated absolute experiment; `TP`.
    REARM,        ///< Packet 19 post-terminal 000F after a named predecessor; `TR`.
    MOVE,         ///< Packet 20 gated move, AxisSession::requestMove or requestMoveRelative; `TG`.
    READ_HISTORY, ///< Packet 4 and packet 22 step 2, 1003 error history reads; `TH`. Not motion.
    // Appended, so that every earlier route keeps the number its records were printed with.
    RELEASED_QUICK, ///< Packet 16 step 3, one 000B on an evidenced released drive; `TQ`.
    WORD67,         ///< Packet 15, words 0006 then 0007 on an evidenced released drive; `TW`.
    RELATIVE,       ///< Packet 21 isolated relative experiment, 005F trigger; `TL`.
    SETTINGS_2202,  ///< Packet 12 T1/T2, one 2202 code and its readback; `TD`. Settings, not motion.
    IO_DI3,         ///< Packet 13 P2/P3, one DI3 change in 2400 and its readback; `TJ`. Settings.
    COUNTER_ZERO,   ///< AxisSession::requestCounterZero, 2302 0, 1, 0; `TZ`. Motion class: it moves the origin.
    CLEAR_ALARM,    ///< AxisSession::requestClearAlarm, 2301 1, 0; `TC`. Motion class.
    MODE_THREE,     ///< Prompt 35, one 6060:00=3 on an evidenced released drive; `TN`. Motion class.
    VELOCITY,       ///< Prompt 35, AxisSession::requestVelocity from released mode 3; `TV`. Motion class.
    HOME,           ///< Prompt 36, AxisSession::requestHome's admission reads and refusal; `TO`. Attended.
    VELOCITY_TARGET, ///< Gate Q03, one 60FF write and its readback on a released mode 3 drive; `TF`. Attended.
    /** The shared console's move arming: MOVE's identity and full read(s) and check, then no write; the
     * console's ProbeMotion is given them with the line's evidence (BenchConsole.h); `TY`. Attended. */
    ARM_MOVE,
    /** The console's velocity arming: VELOCITY's identity, full read and check, then no write; `TB`. Attended. */
    ARM_VELOCITY,
    /** The console's witness arming: two fresh full reads of a stationary drive at the host's reviewed 6064,
     * judged as MOVE judges an idle enabled drive or as RELEASED_QUICK judges a released one, then no write;
     * the console's stepsperturn and axis origin take it as their witness; `TX`. Attended. */
    ARM_WITNESS
};
/** One parsed host request. Fields a route does not use are zero. Ranges are host campaign
 * policy and the documentary native ranges, never a drive timing or safety claim. */
struct RoutePlan {
    RouteKind kind = RouteKind::NONE; ///< Selected route.
    uint8_t node = 0;                 ///< Node 1 or 2, adapter slot node - 1.
    uint32_t trial = 0;               ///< Nonzero trial identity; must rise for this object's life.
    bool attended = false; ///< Motion routes: the host's record of a typed attended confirmation.
    /** ENABLE: the host's record, 1 EnableStart::NO_MOTION_WRITE_RECORDED or 2
     * EnableStart::RELEASED_BY_RECORDED_STOP. Start 2 also reserves the route's own QUICK_STOP after a
     * failure once word 15 may have reached the drive. */
    uint8_t start = 0;
    /** ENABLE: shaft observation, 1 FREE or 2 HOLDING. Start 1: the attendant's typed observation. Start 2:
     * always 1, read from the released start report, never felt. */
    uint8_t shaft = 0;
    uint16_t reviewedStatus = 0;        ///< ENABLE: exact reviewed 6041.
    int32_t reviewedPositionTarget = 0; ///< ENABLE: exact reviewed 607A, history and not intent.
    int32_t reviewedVelocityTarget = 0; ///< ENABLE: exact reviewed 60FF, history and not intent.
    /** ENABLE, STOP, COUNTER_ZERO and CLEAR_ALARM: action deadline after the request. MOVE: the move's
     * immutable deadline after its request, which the library also bounds below. VELOCITY: the run's
     * immutable deadline after its request, beyond its duration; its own stop settles before it.
     * 1..55000. */
    uint32_t deadlineMs = 0;
    /** MODE_ONE, MODE_THREE, POSITION, RELATIVE, REARM, RELEASED_QUICK and WORD67: cycle spacing. ENABLE,
     * STOP, COUNTER_ZERO and CLEAR_ALARM: the action's observation interval. MOVE: the move's
     * MoveOptions::observationIntervalUs, in milliseconds, and the observer's cycle spacing. VELOCITY:
     * VelocityOptions::pollIntervalUs in milliseconds, its stop's observation interval too. */
    uint32_t intervalMs = 0;
    /** Fresh observation cycles: MODE_ONE and MODE_THREE 1..10, for MODE_ONE from a released mode 3 start
     * the most it waits for 6061=1; POSITION, RELATIVE, REARM and RELEASED_QUICK 1..16; WORD67 1..16 after
     * its 0007. */
    uint16_t cycles = 0;
    uint8_t wordCycles = 0;  ///< WORD67: fresh observation cycles after its 0006, 1..4.
    /** REARM: the acknowledged latest word the host expects it to follow on that node: 1 the 001F of a
     * finished POSITION, 2 the 010F of that record's planned HALT, 3 the 005F of a finished RELATIVE. */
    uint8_t predecessor = 0;
    /** SETTINGS_2202: the 2202 code to write, 0 off or 1 after homing (EN86/ZH86 p18). IO_DI3: the case,
     * 1 DI3 function 0 to 7 (USER0), 2 7 to 0, 3 DI3 polarity bit clear to set, 4 set to clear. */
    uint8_t code = 0;
    /** STOP: 1 HALT or 2 QUICK_STOP. POSITION, RELATIVE and MOVE: 0 none, 1 HALT, 2 QUICK_STOP. VELOCITY:
     * 1 HALT or 2 QUICK_STOP, the stop the run asks for at its due time and VelocityEvidence::stopKind. */
    uint8_t stopKind = 0;
    /** STOP, and the planned stop of POSITION, RELATIVE and MOVE: best-effort cutoff, 0 refuses that
     * branch. */
    uint32_t bestEffortMs = 0;
    /** POSITION and RELATIVE: request the stop after this many route cycles, 1..cycles. MOVE: after
     * this many complete move cycles, 1..16, while no completion is observed; a move that ends first is
     * not stopped. */
    uint16_t stopAfter = 0;
    uint32_t stopDeadlineMs = 0; ///< POSITION, RELATIVE and MOVE: the stop's sequenced deadline.
    uint32_t stopIntervalMs = 0; ///< POSITION, RELATIVE and MOVE: the stop's observation interval.
    /** POSITION, RELATIVE and MOVE: exact reviewed 6064 the fresh read, or both fresh reads, must show. */
    int32_t startPosition = 0;
    /** POSITION: 607A, -1000000..1000000 pulses (EN86/ZH86 p11). RELATIVE: 607A as the signed nonzero
     * displacement, in the same range. MOVE: the same range, as an absolute STEPS request in the native
     * frame, or the nonzero relative displacement, quantized exactly. */
    int32_t target = 0;
    /** MOVE: 0 absolute; otherwise relative, with MoveEvidence::relativeBase and the request basis 1
     * ACTUAL or 2 COMMANDED, the drive's base as the host's packet 21 record measured it. */
    uint8_t relativeBase = 0;
    /** MOVE: the relative witness's native position. 0 when absolute; for ACTUAL the start position,
     * which both fresh reads show; for COMMANDED the host's reviewed commanded position, which nothing
     * here can read back. */
    int32_t base = 0;
    /** MOVE: 0, or the other node, read through its own route and session while the move runs, at the
     * move's interval, and never written. It must first read stationary: running and alarm clear, 606C 0. */
    uint8_t observer = 0;
    /** POSITION, RELATIVE: 6081, 0..3000 rpm (EN86/ZH86 p11). MOVE: 1..3000. VELOCITY: the positive 60FF
     * target in motor rpm, 1..3000 (EN86/ZH86 p13), and the evidence's forward maximum; the library
     * refuses above VELOCITY_EXPERIMENT_MAXIMUM_RPM (60) for this attended experiment. */
    uint32_t rpm = 0;
    uint32_t rampUpMs = 0;   ///< POSITION, RELATIVE: 6083, 0..2000 ms (EN86/ZH86 p11). MOVE, VELOCITY: 1..2000.
    uint32_t rampDownMs = 0; ///< POSITION, RELATIVE: 6084, 0..2000 ms (EN86/ZH86 p11). MOVE, VELOCITY: 1..2000.
    /** MOVE: MoveEvidence::record, the nonzero identity of the host's reviewed measurement. VELOCITY:
     * VelocityEvidence::record, the nonzero identity of this attended experiment's own record. */
    uint32_t record = 0;
    uint8_t acceptSource = 0;    ///< MOVE: MoveSignalSource of acceptance, 1 STATUSWORD or 2 MONITOR.
    uint16_t acceptMask = 0;     ///< MOVE: nonzero acceptance mask.
    uint16_t acceptValue = 0;    ///< MOVE: acceptance value, within its mask.
    uint8_t completeSource = 0;  ///< MOVE: MoveSignalSource of completion, 1 STATUSWORD or 2 MONITOR.
    uint16_t completeMask = 0;   ///< MOVE: nonzero completion mask.
    uint16_t completeValue = 0;  ///< MOVE: completion value, within its mask.
    uint8_t rule = 0;            ///< MOVE: CompletionRule, 1 AFTER_ABSENCE or 2 AFTER_ACCEPTANCE.
    /** MOVE and VELOCITY: raw identity of the record, compared exactly with the route's fresh identity
     * read by the library. */
    MotorControlCAN::LICHUAN_C::MoveFirmware firmware;
    /** VELOCITY: FiniteVelocityRequest::durationUs in milliseconds, from the request; below deadlineMs. The
     * library refuses above VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US (20000 ms) for this experiment. */
    uint32_t durationMs = 0;
    /** VELOCITY: VelocityEvidence::layout, the StatusLayout number 1 EN86, 2 ZH86 or 4 OP86: the
     * statusword dialect the run's alarm and running reports are read in. */
    uint8_t layout = 0;
    uint8_t method = 0;          ///< HOME: Lichuan HomingMethod 17, 18, 24 or 29.
    uint32_t searchRpm = 0;      ///< HOME: 6099:01 search rate in motor rpm, 1..3000 (EN86/ZH86 p13).
    uint32_t creepRpm = 0;       ///< HOME: 6099:02 creep rate in motor rpm, 1..3000.
    uint32_t homeRampMs = 0;     ///< HOME: 609A ramp time, 1..2000 ms.
    int32_t offset = 0;          ///< HOME: 607C native offset, -1000000..1000000; no unit claimed.
    /** VELOCITY_TARGET: the signed 60FF value written once, -3000..3001: the documentary 0..3000 rpm
     * (EN86/ZH86 p13), its negation, and 3001, one above it, to see whether the drive range-checks. No
     * rate, unit or direction is claimed for any value, and the drive stays released. */
    int32_t velocityValue = 0;
};
/** @param kind Route. @return True for the shared console's armings, ARM_MOVE, ARM_VELOCITY and ARM_WITNESS. */
inline bool isArming(RouteKind kind) noexcept {
    return kind == RouteKind::ARM_MOVE || kind == RouteKind::ARM_VELOCITY || kind == RouteKind::ARM_WITNESS;
}
/** Parse the text after the `T` command byte: a route letter, then decimal fields separated by
 * commas, with a leading minus only on signed fields and nothing else:
 * `A|I|U|H<node>,<trial>`;
 * `M<node>,<trial>,<attended>,<cycles>,<intervalMs>`;
 * `E<node>,<trial>,<attended>,<start>,<shaft>,<status>,<positionTarget>,<velocityTarget>,
 * <deadlineMs>,<intervalMs>`;
 * `S<node>,<trial>,<attended>,<kind>,<deadlineMs>,<bestEffortMs>,<intervalMs>`;
 * `P|L<node>,<trial>,<attended>,<startPosition>,<target>,<rpm>,<upMs>,<downMs>,<cycles>,<intervalMs>,
 * <stopKind>,<stopAfter>,<stopDeadlineMs>,<stopBestEffortMs>,<stopIntervalMs>`;
 * `R<node>,<trial>,<attended>,<predecessor>,<cycles>,<intervalMs>`;
 * `Q<node>,<trial>,<attended>,<cycles>,<intervalMs>`;
 * `W<node>,<trial>,<attended>,<wordCycles>,<cycles>,<intervalMs>`;
 * `D|J<node>,<trial>,<attended>,<code>`;
 * `Z|C<node>,<trial>,<attended>,<deadlineMs>,<intervalMs>`;
 * `G<node>,<trial>,<attended>,<startPosition>,<target>,<rpm>,<upMs>,<downMs>,<deadlineMs>,<intervalMs>,
 * <record>,<acceptSource>,<acceptMask>,<acceptValue>,<completeSource>,<completeMask>,<completeValue>,
 * <rule>,<1000>,<1009>,<100A>,<1018:01>,<1018:02>,<1018:03>,<2000>,<2001>,<relativeBase>,<base>,
 * <observer>,<stopKind>,<stopAfter>,<stopDeadlineMs>,<stopBestEffortMs>,<stopIntervalMs>`;
 * `N<node>,<trial>,<attended>,<cycles>,<intervalMs>`;
 * `V<node>,<trial>,<attended>,<rpm>,<upMs>,<downMs>,<durationMs>,<intervalMs>,<deadlineMs>,<stopKind>,<record>,
 * <layout>,<1000>,<1009>,<100A>,<1018:01>,<1018:02>,<1018:03>,<2000>,<2001>`;
 * `O<node>,<trial>,<attended>,<method>,<searchRpm>,<creepRpm>,<rampMs>,<offset>`;
 * `F<node>,<trial>,<attended>,<value>`;
 * `Y<node>,<trial>,<attended>,<startPosition>,<intervalMs>,<record>,<acceptSource>,<acceptMask>,<acceptValue>,
 * <completeSource>,<completeMask>,<completeValue>,<rule>,<1000>,<1009>,<100A>,<1018:01>,<1018:02>,<1018:03>,<2000>,
 * <2001>,<relativeBase>,<base>,<stopDeadlineMs>,<stopBestEffortMs>,<stopIntervalMs>`;
 * `B<node>,<trial>,<attended>,<rpm>,<upMs>,<downMs>,<intervalMs>,<record>,<layout>,<stopKind>,<1000>,<1009>,<100A>,
 * <1018:01>,<1018:02>,<1018:03>,<2000>,<2001>,<stopDeadlineMs>,<stopBestEffortMs>,<stopIntervalMs>`;
 * `X<node>,<trial>,<attended>,<startPosition>`
 * (one line each, at most ROUTE_LINE bytes).
 * The host's integrity field is not part of this text: checkRouteIntegrity removes it first.
 * @param text Borrowed characters; no terminator required.
 * @param size Character count.
 * @param output Published only on success.
 * @return False for anything else, including a value outside its stated range or a plan
 * routePlanValid refuses.
 */
bool parseRouteCommand(const char* text, size_t size, RoutePlan& output) noexcept;
/** The parser's own range and consistency check of a plan, however it was built: every field the
 * route uses within its stated range, a planned stop naming all of its policy and no stop naming
 * none, a move signal value within its mask, a nonzero relative displacement, and a move base that
 * is 0 when absolute and the start position for ACTUAL; a velocity duration below its deadline and a
 * layout other than 3 (ZH57, no 86C dialect); a home method of 17, 18, 24 or 29; an enable whose start 2
 * (released by a recorded stop) carries shaft 1. Fields the route does not use are not judged.
 * @param plan Borrowed for this call.
 * @return True when parseRouteCommand could have produced these route fields.
 */
bool routePlanValid(const RoutePlan& plan) noexcept;
/** CRC-32 of a route line as the host computes it: IEEE 802.3, reflected polynomial 0xEDB88320,
 * initial and final value 0xFFFFFFFF, as Python's zlib.crc32, over the command byte `T` and then
 * the text. Bit by bit, with no table.
 * @param text Borrowed characters after `T`.
 * @param size Character count.
 * @return The checksum.
 */
uint32_t routeLineCrc(const char* text, size_t size) noexcept;
/** Check the host's integrity field before anything parses the line: the text after `T` must end in
 * `,K` and eight lowercase hexadecimal digits holding routeLineCrc of every byte before `,K`. A digit
 * corrupted in transit that still parses as another valid plan fails here.
 * @param text Borrowed characters after `T`, integrity field included.
 * @param size Character count, at most ROUTE_LINE.
 * @param body Set to the length of the route text before `,K`; written only on success.
 * @return False when the field is absent, malformed or does not match.
 */
bool checkRouteIntegrity(const char* text, size_t size, size_t& body) noexcept;
/** @param kind Route. @return Static lowercase name printed in records; "none" for NONE. */
const char* routeName(RouteKind kind) noexcept;
/** Why a route ended, as printed in its header. Device facts stay in the terminals. */
enum class RouteFailure : uint8_t {
    NONE,          ///< Every planned step completed with success evidence; not a qualification.
    AUTHORIZATION, ///< A one-use grant was refused; that upload was not admitted.
    ADMISSION,     ///< The session or owner refused a step, or a context refused preparation.
    SERVICE,       ///< A terminal other than success; kept, with any uncertainty it carries.
    CORRELATION,   ///< A settled terminal was not the request this route admitted.
    /** Copied traffic or terminals were lost; no further step was admitted, and an active enable or
     * move was withdrawn locally. Never sent anything itself. */
    EVIDENCE_LOSS,
    INVALIDATED,   ///< Reservation, node epoch or firmware progression ended before the next step.
    DEADLINE,      ///< The route cutoff or an operation deadline was reached between steps.
    PREREQUISITE,  ///< A fresh read before any write did not meet the route's stated condition.
    OBSERVATION,   ///< After a write, a fresh read or a nonzero EMCY met a stated stop condition.
    ACTION,        ///< The session's enable, move, velocity or stop ended without its success outcome.
    /** The library refused the enable, move, velocity, home or stop request; nothing of it was sent. HOME
     * ends here by design. */
    REFUSED,
    ABORTED        ///< Local abort; no stop was sent.
};
/** Local resource sample; the SDK calls behind it live in the firmware entry. */
struct RouteResources {
    uint32_t stackFree = 0;   ///< Least bytes ever free on the owner task's stack.
    uint32_t heapFree = 0;    ///< Internal heap bytes free now.
    uint32_t heapMinimum = 0; ///< Least internal heap bytes ever free.
    uint32_t largestBlock = 0; ///< Largest internal heap block free now: the next session needs one.
    uint32_t txErrors = 0;    ///< Controller TX error counter when sampled.
    uint32_t rxErrors = 0;    ///< Controller RX error counter when sampled.
};
/** Optional sampling, called at admission and at the terminal only. */
struct RouteCallbacks {
    void* context = nullptr; ///< Borrowed for this object's lifetime.
    void (*sample)(void*, RouteResources&) = nullptr; ///< Fills a zeroed sample; bounded, no I/O.
};
/** Fixed capacities and campaign caps. */
enum {
    ROUTE_TERMINALS = 128,      ///< Copied owner terminals; a plan that could exceed it is refused.
    ROUTE_TRAFFIC = 448,        ///< Copied wire and controller records; overflow is evidence loss.
    ROUTE_TAIL = 16,            ///< Records copied after the terminal, until release.
    ROUTE_MOTION_TOTAL = 12,    ///< Motion admissions per boot (packet 22).
    ROUTE_MOTION_PER_CASE = 3,  ///< Motion admissions per route and node per boot (packet 22).
    /** Settings, HOME and VELOCITY_TARGET admissions per route and node per boot; not motion. */
    ROUTE_SETTINGS_PER_CASE = 3,
    ROUTE_CUTOFF_MS = 60000,    ///< Route cutoff after admission (packet 22: sixty seconds a case).
    ROUTE_QUEUE_US = 100000,    ///< Queue budget per service, host policy.
    ROUTE_RESPONSE_US = 250000, ///< Response budget per service, host policy as for the soak.
    ROUTE_OPERATION_US = 5000000, ///< Deadline of one read or write operation, clamped to the cutoff.
    /** VELOCITY's final-enable report window, VelocityOptions::enableWindowUs in milliseconds (backlog
     * S17): the attended experiment's chosen budget, about twice the 850 to 880 ms mode 1 lag of
     * 2026-11-09, never a drive timing. */
    ROUTE_VELOCITY_WINDOW_MS = 2000,
    /** VELOCITY's spacing between window cycles, VelocityOptions::enableSpacingUs in milliseconds: the
     * smallest whose six extra cycles let a 12 s run at 5 s intervals fit ROUTE_TERMINALS. */
    ROUTE_VELOCITY_SPACING_MS = 300,
    /** Most extra seven-field cycles the window adds: ROUTE_VELOCITY_WINDOW_MS / ROUTE_VELOCITY_SPACING_MS. */
    ROUTE_VELOCITY_WINDOW_CYCLES = ROUTE_VELOCITY_WINDOW_MS / ROUTE_VELOCITY_SPACING_MS,
    /** Longest request text after `T` the firmware collects, integrity field included; a longer one
     * is refused. The longest move line with its field is 220 bytes. */
    ROUTE_LINE = 256
};
/** First grant identity; RawEvidence uses 1000..4029 and ProbeApp reads count from 1. */
constexpr uint32_t ROUTE_GRANT_BASE = 100000;
/** Work of SETTINGS_2202, IO_DI3, VELOCITY, HOME and a MOVE with an observer node, which the caller places once,
 * beside BenchRoute's own block, as it places a session: these contexts are too large to add to that
 * block. Written only while such a route runs; its contents are history afterwards.
 */
struct RouteWork {
    /** The fresh group read the candidate is built and checked against: SOFT_LIMITS or
     * IO_INPUT_CONFIGURATION. HOME: its IO_INPUT_CONFIGURATION read, requestHome's input mapping. */
    MotorControlCAN::LICHUAN_C::DriverContext prior;
    MotorControlCAN::LICHUAN_C::DriverSettingsContext transaction; ///< The one write and its readback.
    /** The session's reservation as the transaction holds it: the owner's sequence, acquired before
     * the prerequisite reads. Unchanged for the whole transaction. */
    MotorControlCAN::LICHUAN_C::AxisReservation live;
    /** The axis binding, which the write's one pre-transmit notice advances to the next configuration
     * epoch; the owner's sequence and the session then follow it. */
    MotorControlCAN::Binding binding;
    MotorControlCAN::LICHUAN_C::DriverObservation limits; ///< Scratch for getDriver.
    MotorControlCAN::LICHUAN_C::IoObservation io;         ///< Scratch for getIo.
    /** MOVE with an observer: the other node's state reads, on that node's own route and session. HOME:
     * its full state read, kept for requestHome while DRIVER is read after it. */
    MotorControlCAN::LICHUAN_C::ReadContext observer;
    /** VELOCITY: requestVelocity's inputs, built from the line and borrowed for that call only; the
     * session copies them. Here rather than in BenchRoute, whose block is the largest the heap gives. */
    MotorControlCAN::LICHUAN_C::VelocityEvidence velocityEvidence;
    MotorControlCAN::FiniteVelocityRequest velocityRequest; ///< VELOCITY: the rate and duration.
    MotorControlCAN::LICHUAN_C::VelocityProfile velocityProfile; ///< VELOCITY: the native ramps.
    MotorControlCAN::LICHUAN_C::VelocityOptions velocityOptions; ///< VELOCITY: the route's budgets.
    /** HOME: requestHome's inputs, borrowed for that call only. The evidence stays CLOSED. */
    MotorControlCAN::LICHUAN_C::HomeEvidence homeEvidence;
    MotorControlCAN::LICHUAN_C::HomeRequest homeRequest; ///< HOME: the line's method, rates, ramp, offset.
    MotorControlCAN::LICHUAN_C::HomeOptions homeOptions; ///< HOME: the route's budgets.
};
/** One owner terminal, compacted from Completion for the RESULT line. */
struct RouteTerminal {
    uint64_t admittedUs = 0;    ///< Native admission time.
    uint64_t completedUs = 0;   ///< Native terminal time.
    uint32_t operation = 0;     ///< Transfer operation.
    uint32_t request = 0;       ///< Transfer request.
    uint32_t ownerRequest = 0;  ///< Owner result handle generation.
    uint32_t step = 0;          ///< Transfer step.
    uint32_t abortCode = 0;     ///< Raw abort code.
    uint32_t nodeGeneration = 0; ///< Node epoch at admission.
    uint16_t index = 0;         ///< Object index.
    uint8_t subindex = 0;       ///< Object subindex.
    uint8_t direction = 0;      ///< SdoDirection.
    uint8_t outcome = 0;        ///< SdoOutcome.
    uint8_t flags = 0;          ///< Bit0 acknowledged, bit1 effect possible, bit2 uncertain, bit3 cleanup.
    uint8_t size = 0;           ///< Uploaded bytes.
    uint8_t reason = 0;         ///< Owner CompletionReason.
    uint8_t stages = 0;         ///< Stage bits as the RESULT line's st field.
    uint8_t node = 0;           ///< Node of the transfer's channel: a record may hold two.
    uint8_t data[4] = {};       ///< First four uploaded bytes.
};
/** One owner diagnostic other than a terminal, compacted from Diagnostic for the TRAFFIC line. */
struct RouteTraffic {
    uint64_t processedUs = 0; ///< Owner processing time.
    uint64_t receivedUs = 0;  ///< RX capture when exact.
    uint64_t earliestUs = 0;  ///< RX lower bound when bounded.
    uint64_t latestUs = 0;    ///< RX upper bound when bounded.
    uint32_t id = 0;          ///< Frame identifier.
    uint32_t epoch = 0;       ///< Controller epoch.
    uint32_t txRequest = 0;   ///< Local TX identity: SDO request or NMT generation.
    uint32_t txFrame = 0;     ///< Local TX frame number.
    uint8_t data[8] = {};     ///< Frame bytes.
    uint8_t kind = 0;         ///< DiagnosticKind.
    uint8_t dlc = 0;          ///< Wire DLC.
    uint8_t flags = 0;        ///< Bit0 ext, bit1 rtr, bit2 accepted, bit3 exact time, bit4 bounds.
    uint8_t state = 0;        ///< ControllerState or RecoveryOutcome, by kind.
    uint8_t txKind = 0;       ///< TransportTxId kind.
    uint8_t txChannel = 0;    ///< SDO channel of the local TX identity, which names its node.
};
/** Raw values of one fresh state read, kept for the prerequisite check and its summary line. */
struct RouteSnapshot {
    uint8_t blocks = 0;        ///< StateBlock bits present.
    uint8_t errorRegister = 0; ///< 1001.
    uint16_t errorCode = 0;    ///< 603F.
    uint16_t monitor = 0;      ///< 2100.
    uint16_t statusword = 0;   ///< 6041.
    uint8_t mode = 0;          ///< 6061 complete byte.
    int32_t position = 0;      ///< 6064 raw.
    int32_t velocity = 0;      ///< 606C raw.
    uint16_t logicalInputs = 0; ///< 2101 raw, with MONITORS; not printed in the snapshot lines.
};
/** Owned record of one route; only count prefixes are valid. Terminal fields never change after
 * the terminal; tail copies continue until release. */
struct RouteRecord {
    RoutePlan plan;                            ///< Copied host request.
    RouteFailure failure = RouteFailure::NONE; ///< First terminal cause.
    const char* why = "none";                  ///< Static token naming the condition.
    bool terminal = false;                     ///< The route has ended.
    bool evidenceLost = false;                 ///< Traffic or terminal capacity or owner trace lost.
    /** Evidence was lost while an enable or move was active, and this route withdrew it locally
     * through cancelEnable or cancelMove. Nothing was sent because of it, and no stop. */
    bool lossCancel = false;
    bool tailOverflow = false;                 ///< Tail capacity or owner trace lost after terminal.
    bool unsettled = false; ///< The route ended with its own request not settled (firmware failure).
    uint8_t step = 0;       ///< Route step index reached.
    uint8_t motionTotal = 0; ///< Motion admissions this boot, including this one.
    uint8_t caseCount = 0;   ///< Motion admissions of this route and node this boot.
    uint16_t cycles = 0;     ///< Completed observation cycles.
    uint64_t startedUs = 0, completedUs = 0; ///< Admission and terminal times.
    uint64_t maxServiceGapUs = 0;            ///< Largest interval between service calls.
    RouteResources before; ///< Sample at admission.
    RouteResources after;  ///< Sample at the terminal: stackFree is the least ever, so a lower
                           ///< value than before was reached during this route.
    uint32_t dropped = 0;  ///< Lost records, saturating.
    uint8_t admission = 0; ///< BusAdmission of the last refused admission; meaningful for ADMISSION.
    uint8_t terminalCount = 0; ///< Valid terminals.
    uint16_t trafficCount = 0; ///< Valid traffic.
    uint8_t tailCount = 0;     ///< Valid tail.
    RouteTerminal terminals[ROUTE_TERMINALS]; ///< Every owner terminal in publication order.
    RouteTraffic traffic[ROUTE_TRAFFIC];      ///< Every other owner diagnostic in order.
    RouteTraffic tail[ROUTE_TAIL];            ///< Diagnostics after the terminal.
    bool checkPresent = false; ///< A prerequisite check ran on snapshot below.
    bool checkPassed = false;  ///< That check passed.
    /** RELEASED_QUICK, WORD67 and a relative MOVE read the state twice before their check: the second
     * read has begun, and `first` holds the first. The check judges both. */
    bool firstPresent = false;
    RouteSnapshot first;       ///< The first of the two fresh check reads.
    RouteSnapshot check;       ///< Values the check judged; with two reads, the second.
    /** Latest successful fresh read of the blocks RouteSnapshot holds. A read of ERROR_HISTORY
     * alone leaves it unchanged. */
    RouteSnapshot last;
    /** READ_HISTORY: the 1003 read succeeded and the library published it; the next two fields hold
     * it. A failed read leaves this false, with its raw terminals in the RESULT lines. */
    bool historyPresent = false;
    uint8_t historyCount = 0; ///< 1003:00 raw, 0..4: the library refuses a larger count.
    uint32_t history[4] = {}; ///< 1003:01.. raw; only the first historyCount entries were read.
    bool statusPresent = false; ///< A library request was refused; the next fields hold it.
    uint8_t statusCode = 0;     ///< Err of that refusal.
    int32_t statusDetail = 0;   ///< Detail of that refusal.
    const char* statusMessage = ""; ///< Static message of that refusal.
    bool enableRequested = false;   ///< requestEnable accepted.
    bool stopRequested = false;     ///< requestStop returned a path other than NONE.
    uint8_t stopRequestKind = 0;    ///< StopKind asked for.
    uint8_t stopPathRequested = 0;  ///< StopPath requestStop returned.
    /** When the enable or stop was requested; an ENABLE route's own stop keeps the enable's time. */
    uint64_t actionRequestedUs = 0;
    bool emcy = false;              ///< A nonzero EMCY of this node was copied.
    uint8_t emcyData[8] = {};       ///< Its bytes.
    bool moveRequested = false;     ///< MOVE: requestMove accepted; AxisSession::moveAction holds it.
    uint8_t movePhases = 0;         ///< MOVE: bit per MovePhase seen at a service turn while active.
    uint64_t moveRequestedUs = 0;   ///< MOVE: when requestMove was accepted.
    uint32_t rearmFollows = 0;      ///< REARM: operation of the acknowledged predecessor it followed.
    uint64_t rearmFollowsUs = 0;    ///< REARM: that predecessor terminal's completion time.
    uint16_t rearmFollowsWord = 0;  ///< REARM: that predecessor's word.
    /** The session's latest acknowledged controlword at admission, whatever it was; none after an MCU
     * restart. RELEASED_QUICK and WORD67 print it and require it unchanged at their first write. */
    bool priorPresent = false;
    uint16_t priorWord = 0;      ///< Its word.
    uint32_t priorOperation = 0; ///< Its operation.
    uint64_t priorUs = 0;        ///< Its terminal's completion time.
    bool targetReadPresent = false; ///< A 607A upload of this record succeeded; targetRead holds the last.
    int32_t targetRead = 0;         ///< That 607A, raw signed.
    /** RELATIVE and a relative MOVE: at admission, this object's own record of the latest acknowledged
     * 607A download on that node, by a POSITION, RELATIVE or MOVE of this boot and node epoch. Absent
     * after any 607A that may have been written unacknowledged. */
    bool commandedKnown = false;
    int32_t commanded = 0;          ///< That 607A: an absolute target, or a displacement when relative.
    uint32_t commandedTrial = 0;    ///< The trial that wrote it.
    bool commandedRelative = false; ///< It was a displacement, with a 005F trigger.
    uint8_t witnessBasis = 0;       ///< Relative MOVE: the witness's basis, 1 ACTUAL or 2 COMMANDED.
    int32_t witnessPosition = 0;    ///< Relative MOVE: the witness's native position.
    /** SETTINGS_2202 and IO_DI3: the library prepared the candidate; the fields below hold its result. */
    bool settingsPrepared = false;
    /** The candidate equalled the prior readback: the library succeeded with nothing to write, and
     * nothing was sent. */
    bool settingsNoop = false;
    uint8_t settingsState = 0;      ///< SettingsState at the record's end.
    uint8_t settingsOutcome = 0;    ///< ReadOutcome at the record's end.
    uint16_t settingsChanged = 0;   ///< Fields the candidate changed, by group position.
    uint16_t settingsConfirmed = 0; ///< Of those, the writes acknowledged.
    bool settingsEffects = false;   ///< A write may have reached the drive.
    bool settingsUncertain = false; ///< An effect is unconfirmed.
    uint32_t configurationBefore = 0; ///< The binding's configuration epoch before the transaction.
    uint32_t configurationAfter = 0;  ///< After it: one more once a write may have left.
    /** The owner's reservation and the session follow configurationAfter: later routes on this node
     * read at the new epoch. */
    bool configurationFollowed = false;
    /** Raw group values, 0 the prior read and 1 the matched readback; present only when published. */
    bool groupPresent[2] = {false, false};
    uint16_t limitSelector[2] = {0, 0}; ///< SETTINGS_2202: 2202.
    uint16_t limitEntries[2] = {0, 0};  ///< SETTINGS_2202: 607D:00, raw as read (two bytes, MEASURED_U16).
    int32_t limitMinimum[2] = {0, 0};   ///< SETTINGS_2202: 607D:01.
    int32_t limitMaximum[2] = {0, 0};   ///< SETTINGS_2202: 607D:02.
    uint16_t ioEntries[2] = {0, 0};     ///< IO_DI3: 2400:00.
    uint16_t ioPolarity[2] = {0, 0};    ///< IO_DI3: 2400:01, every bit.
    uint16_t ioFunctions[2][7] = {};    ///< IO_DI3: 2400:02..08, DI0..6 raw.
    /** IO_DI3: one fresh MONITORS and INPUTS read after the readback; the next two fields hold it. */
    bool inputsPresent = false;
    uint16_t logicalInputs = 0; ///< 2101 raw.
    uint32_t digitalInputs = 0; ///< 60FD raw.
    /** MOVE with an observer: its check read was judged (observerPassed), and why it failed. */
    bool observerChecked = false;
    bool observerPassed = false;       ///< The observer read stationary before the move.
    const char* observerWhy = "none";  ///< Static token of the observer check, or why observing stopped.
    uint16_t observerCycles = 0;       ///< Complete observer cycles while the move or its stop ran.
    bool observerStopped = false;      ///< Observing ended early: a failed read, an EMCY or the cutoff.
    /** A cycle read 606C nonzero, 2100 running, or a 6064 other than the check's: a report, not a judgment. */
    bool observerMoved = false;
    int32_t observerMinimum = 0, observerMaximum = 0; ///< Least and greatest 6064 of the cycles.
    int32_t observerVelocity = 0;      ///< Largest magnitude of 606C in the cycles.
    uint16_t observerMonitors = 0;     ///< 2100 of every cycle, combined with OR.
    bool observerEmcy = false;         ///< A nonzero EMCY of the observer node was copied.
    uint8_t observerEmcyData[8] = {};  ///< Its bytes.
    RouteSnapshot observerCheck;       ///< The observer's check read.
    RouteSnapshot observerLast;        ///< Its latest complete cycle.
    /** Copied RX frames per node, 1 and 2, and those the owner's dispatch did not accept. */
    uint16_t nodeRx[2] = {0, 0}, nodeUnmatched[2] = {0, 0};
    /** The owner's whole-interface counters at admission and at the terminal: rxDropped,
     * unmatchedFrames and rxLost. A dropped frame has no node. */
    uint32_t droppedBefore = 0, droppedAfter = 0, unmatchedBefore = 0, unmatchedAfter = 0;
    uint32_t lostBefore = 0, lostAfter = 0;
    /** COUNTER_ZERO and CLEAR_ALARM: requestCounterZero or requestClearAlarm accepted;
     * AxisSession::clearAction holds it. */
    bool clearRequested = false;
    /** VELOCITY: requestVelocity accepted; AxisSession::velocityAction holds it, and the stop it asked for
     * at its due time is the session's stopAction of the plan's kind. */
    bool velocityRequested = false;
    uint64_t velocityRequestedUs = 0; ///< VELOCITY: when requestVelocity was accepted.
    /** HOME: the identity read was published and the IO_INPUT_CONFIGURATION group (groupPresent[0],
     * ioEntries[0], ioPolarity[0], ioFunctions[0]) and the full read (last) are held. */
    bool homeIdentity = false;
    bool homeDriverPresent = false; ///< HOME: the DRIVER group was published; the next two fields hold it.
    uint16_t homeDirection = 0;     ///< HOME: 2200 raw.
    uint16_t homeSubdivision = 0;   ///< HOME: 2201 raw.
    /** HOME: requestHome returned OK. It cannot with the closed gate; the route then withdraws it at once,
     * before any service could admit its first write. */
    bool homeAdmitted = false;
    /** VELOCITY_TARGET: the 60FF download's terminal settled; the next four fields hold it. */
    bool probeWritten = false;
    uint8_t probeOutcome = 0;       ///< Its SdoOutcome.
    bool probeAcknowledged = false; ///< The server acknowledged the download.
    bool probeUncertain = false;    ///< Its effect is unconfirmed, as after a lost reply.
    uint32_t probeAbort = 0;        ///< Its raw abort code, 0 without one.
    /** VELOCITY_TARGET: the 60FF upload after the acknowledged download succeeded; the next two fields
     * hold it raw. */
    bool probeReadPresent = false;
    uint8_t probeSize = 0;          ///< Uploaded byte count.
    uint8_t probeRead[4] = {};      ///< The uploaded bytes as on the wire, least significant first.
    /** VELOCITY_TARGET: 0 no full read after the readback was judged, 1 it reported the check's
     * standstill, 2 it did not. */
    uint8_t probeAfter = 0;
};
/** BenchRoute::kept bits: facts that refuse later work and survive closeSessions. */
enum SessionKept : uint8_t {
    KEPT_MOVE_FENCE = 1,     ///< AxisSession::moveFenced: a trigger may have moved without an observed completion.
    KEPT_CLEAR_BLOCK = 2,    ///< AxisSession::clearBlocked: a clear's value 1 may still be held.
    KEPT_FAULT_BLOCK = 4,    ///< AxisSession::faultBlocked: work was withdrawn for a drive fault.
    KEPT_VELOCITY_STOP = 8,  ///< AxisSession::velocityNeedsStop.
    KEPT_HOME_STOP = 16,     ///< AxisSession::homeNeedsStop.
    KEPT_WRITTEN = 32,       ///< AxisSession::motionHistoryWritten: the enable stays refused.
    KEPT_MODE_WRITE = 64     ///< This object admitted a 6060 write on the node: MODE_ONE stays refused.
};
class BenchRoute;
/** A successful arming record's acquisition (isArming), as BenchRoute::armed publishes it. The
 * pointers name the route's own read storage: valid until the next route begins, which ends armed(). */
struct RouteArming {
    RoutePlan plan;                                           ///< The arming line, copied at its terminal.
    const MotorControlCAN::LICHUAN_C::ReadContext* state = nullptr; ///< The checked full read (the second, if two).
    const MotorControlCAN::LICHUAN_C::IdentityObservation* identity = nullptr; ///< The checked identity read.
    int32_t position = 0;    ///< 6064 of the checked read, raw.
    uint64_t feedbackUs = 0; ///< Earliest bound of that read's 6064 and 606C.
};
/** Format the console reply to the bench's session close (`x`), with its newline:
 * `SESSIONS closed node1=.. node2=.. kept1=.. kept2=.. motion=..; ...` or
 * `SESSIONS refused reason=.. node1=.. node2=..; ...`.
 * @param route The route object, or null when none exists yet (no route ran this boot).
 * @param refusal Null for a close, else the refusal token.
 * @param closed closeSessions's mask.
 * @param output Destination. @param size Its capacity; 192 bytes hold every line.
 * @return False when the line does not fit; output is then empty when size is nonzero.
 */
bool formatSessionsLine(const BenchRoute* route, const char* refusal, uint8_t closed, char* output,
                        size_t size) noexcept;
/** One attended route at a time over borrowed sessions. Large and stationary; allocates nothing.
 * The caller places it, attaches one session per node before that node's first route, services the
 * firmware and then this object each turn, and prints records with formatLine.
 */
class BenchRoute {
  public:
    /** Borrow stationary resources; construction does nothing else.
     * @param firmware Assembly whose application and owner carry every route.
     * @param services That assembly's service store, with routes the caller configured.
     * @param callbacks Optional resource sampling.
     */
    BenchRoute(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services,
               const RouteCallbacks& callbacks) noexcept;
    BenchRoute(const BenchRoute&) = delete;            ///< Holds live evidence and borrowed sessions.
    BenchRoute& operator=(const BenchRoute&) = delete; ///< Holds live evidence and borrowed sessions.
    /** Attach the stationary session of one node; it is opened on that node's first route.
     * @param node Node 1 or 2.
     * @param session Closed session on this firmware's owner, outliving this object.
     * @return False when the node is invalid or already has one.
     */
    bool attach(unsigned node, AxisSession& session) noexcept;
    /** Attach the stationary settings storage, before the first settings route.
     * @param work Outliving this object; written only while a settings route runs.
     * @return False when one is already attached.
     */
    bool attachWork(RouteWork& work) noexcept;
    /** @return Whether settings storage is attached. */
    bool workAttached() const noexcept { return work_ != nullptr; }
    /** @param node Node 1 or 2. @return Whether that node has a session attached. */
    bool attached(unsigned node) const noexcept {
        return node >= 1 && node <= 2 && sessions_[node - 1] != nullptr;
    }
    /** @param node Node 1 or 2. @return The attached session, or null. */
    const AxisSession* session(unsigned node) const noexcept {
        return attached(node) ? sessions_[node - 1] : nullptr;
    }
    /** Admit one route. Sends nothing during this call.
     * @param plan Copied request; routePlanValid judges it again, however it was built.
     * @param nowUs Owner time, at least its last time.
     * @return Null when admitted; otherwise a static refusal token ("plan" for a plan routePlanValid
     * refuses), with nothing changed except that a session opened here stays open.
     */
    const char* begin(const RoutePlan& plan, uint64_t nowUs) noexcept;
    /** One turn after the firmware's: copy diagnostics, settle, check, admit at most one step.
     * @param nowUs Owner time; an earlier value is ignored for this turn.
     */
    void service(uint64_t nowUs) noexcept;
    /** Withdraw the active route locally. Sends no stop. A request on its way settles first; an
     * active enable or move is withdrawn through AxisSession::cancelEnable or cancelMove.
     * @param nowUs Owner time.
     * @return Null when the route will end, ABORTED unless an EMCY or lost evidence already decided
     * its failure; otherwise a static refusal token. A stop in progress refuses: it settles by itself.
     */
    const char* abort(uint64_t nowUs) noexcept;
    /** Release a terminal record once local TX has retired. Sessions stay open.
     * @return False while active, with nothing retained, or while local TX is outstanding.
     */
    bool release() noexcept;
    /** Close every open, idle route session (backlog S15), so that its owner reservation returns and
     * the K and H records, or the raw and PDO experiments, can reserve that node again in this boot.
     * Sends nothing and changes nothing on a drive: AxisSession::close only, which releases the
     * reservation the session took. It is not a stop, release or disable. Each closed session drops
     * its latest controlword; a later route reopens the same session object, so its fences and
     * write history refuse as before, and this object's trial identity, caps and counters stay.
     * @param closed Set to the nodes this call closed, bit 0 node 1 and bit 1 node 2; zero on every
     * refusal but "owner".
     * @return Null when every open session closed, or none was open. Otherwise a static token, with
     * nothing closed: "record" while a route record is held, active or terminal and unreleased;
     * "state" while the firmware's own read is active or its request unreleased; "busy" while a
     * session has a stop, enable, move, velocity, home or clear in progress, an admitted request or
     * an unread result, or its node's local TX has not retired; "configuration" while a counter
     * clear's or home's move of the owner to its new configuration epoch is still owed, which
     * AxisSession::close would drop. "owner" if the owner refused a release after the checks;
     * closed then names the sessions already closed.
     */
    const char* closeSessions(uint8_t& closed) noexcept;
    /** What a session object and this object keep for a node across close and reopen, as SessionKept
     * bits: the facts that refuse later work.
     * @param node 1 or 2. @return Zero for an invalid node or one with no session attached.
     */
    uint8_t kept(unsigned node) const noexcept;
    /** @return Borrowed record from admission until release, or null. */
    const RouteRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** @return True from admission until the terminal. */
    bool active() const noexcept { return occupied_ && !record_.terminal; }
    /** Authorize only an exact pending download of this route or an attached session.
     * Calls the session preflight and required settings notice once at owner handoff.
     * @param sequence Reservation identity. @param transfer Complete pending work.
     * @param now Supplied owner time. @return False for unmatched or withdrawn work.
     */
    bool preflight(MotorControlCANOwner::SequenceId sequence,
                   const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t now) noexcept;
    /** @return Motion admissions this boot, the shared console's included. */
    uint8_t motionAdmissions() const noexcept { return motionTotal_; }
    /** The last route's arming, while nothing has replaced its reads.
     * @param output Set on true. @return True after a successful arming terminal (isArming), until
     * the next admitted route or a closeSessions that closed a session. Sends nothing. */
    bool armed(RouteArming& output) const noexcept;
    /** Whether the per-boot motion caps admit one console motion of this kind on this node, as begin judges
     * a route of that kind: a velocity counts its own stop against STOP as well.
     * @param node 1 or 2. @param kind MOVE, VELOCITY, ENABLE, COUNTER_ZERO or CLEAR_ALARM.
     * @return False for anything else or over a cap. */
    bool consoleMotionAllowed(unsigned node, RouteKind kind) const noexcept;
    /** Count one admitted console motion of this kind, as begin counts a route; a console STOP is counted
     * too, saturating, and never refused. @param node 1 or 2. @param kind As above, or STOP. */
    void countConsoleMotion(unsigned node, RouteKind kind) noexcept;
    /** Format one line of the terminal record: header, summaries, RESULT, TRAFFIC, TAIL, END.
     * @param cursor Zero-based line index.
     * @param output Destination.
     * @param size Its capacity, at least 256 bytes for every line to fit.
     * @return False past the END line or before the terminal; output is then untouched.
     */
    bool formatLine(unsigned cursor, char* output, size_t size) const noexcept;

  private:
    enum StepKind : uint8_t {
        STEP_STATE, STEP_GROUP, STEP_TUNING, STEP_SINGLE, STEP_CHECK_MODE, STEP_CHECK_POSITION,
        STEP_MODE_WRITE, STEP_ENABLE, STEP_STOP, STEP_POSITION_WRITES, STEP_OBSERVE, STEP_END,
        STEP_IDENTITY, STEP_CHECK_REARM, STEP_WORD_WRITE, STEP_CHECK_MOVE, STEP_MOVE,
        STEP_SECOND,         ///< A second fresh state read; the first is kept in RouteRecord::first.
        STEP_CHECK_RELEASED, ///< RELEASED_QUICK's and WORD67's check of both reads.
        STEP_PRIOR,          ///< A group read kept in RouteWork::prior.
        STEP_SETTINGS,       ///< The settings candidate, its write and readback.
        STEP_OBSERVER_CHECK, ///< MOVE: the observer node's fresh read and stationary check.
        STEP_CHECK_ALARM,    ///< CLEAR_ALARM: the fresh read must report the alarm.
        STEP_CLEAR,          ///< COUNTER_ZERO and CLEAR_ALARM: the session's clear request.
        STEP_CHECK_MODE_THREE, ///< MODE_THREE's check of both reads: released, stationary, mode 0 or 1.
        STEP_CHECK_VELOCITY, ///< VELOCITY's check: released and stationary in mode 3.
        STEP_VELOCITY,       ///< VELOCITY: the session's velocity request.
        STEP_HOME_STATE,     ///< HOME: a full state read kept in RouteWork::observer.
        STEP_HOME,           ///< HOME: the session's home request, refused before any write.
        STEP_CHECK_TARGET,   ///< VELOCITY_TARGET's check of both reads: released standstill in mode 3.
        STEP_TARGET_WRITE,   ///< VELOCITY_TARGET: the one 60FF download.
        STEP_CHECK_TARGET_AFTER, ///< VELOCITY_TARGET: the read after the readback against the check.
        /** MODE_ONE: a second fresh state read, kept as STEP_SECOND keeps it, only when the first read
         * reported mode 3; skipped otherwise. */
        STEP_SECOND_RELEASED,
        STEP_CHECK_WITNESS ///< ARM_WITNESS's check of both reads: stationary, idle or released, at the reviewed 6064.
    };
    struct Step {
        uint8_t kind;
        uint8_t arg;
    };
    // RX precedes TX within an owner turn, before the route has consumed its diagnostic copy.
    bool unhandledFault(uint8_t slot) const noexcept;
    void noteEmcyHandled() noexcept;
    FirmwareProbe& firmware_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    RouteCallbacks callbacks_;
    AxisSession* sessions_[2] = {nullptr, nullptr};
    const Step* steps_ = nullptr;
    // Scratch and work outside RouteRecord: too large for the task stack.
    MotorControlCAN::LICHUAN_C::DriverContext context_;
    MotorControlCAN::LICHUAN_C::StateObservation block_;
    MotorControlCANOwner::Diagnostic diagnostic_;
    MotorControlCAN::CANOPEN::SdoTransfer expected_, single_;
    // MOVE: the identity read the move is checked against, and each node's coordinate configuration
    // and relative witness, which the session borrows until that node's move is terminal. Written only
    // while that node's move is idle.
    MotorControlCAN::LICHUAN_C::IdentityObservation identity_;
    MotorControlCAN::AxisConfig moveAxes_[2];
    MotorControlCAN::AxisReference moveReferences_[2];
    // requestMove's other inputs, borrowed for that call only; kept here, off the owner task's stack.
    MotorControlCAN::LICHUAN_C::MoveEvidence moveEvidence_;
    MotorControlCAN::MoveRequest moveRequest_;
    MotorControlCAN::LICHUAN_C::MoveProfile moveProfile_;
    MotorControlCAN::LICHUAN_C::MoveOptions moveOptions_;
    RouteRecord record_;
    uint64_t lastUs_ = 0, cutoffUs_ = 0, nextCycleUs_ = 0, cycleStartUs_ = 0, writeDeadlineUs_ = 0;
    uint32_t lastTrial_ = 0, grant_ = ROUTE_GRANT_BASE - 1, grantedOperation_ = 0, grantedStep_ = 0;
    uint32_t writeOperation_ = 0, overwritten_ = 0, rxLost_ = 0;
    // Each node's latest acknowledged POSITION or RELATIVE trigger, 001F or 005F, and the 010F of a
    // finished POSITION's planned HALT: the only words a REARM may follow.
    uint32_t triggerOperation_[2] = {0, 0}, haltOperation_[2] = {0, 0};
    uint64_t triggerUs_[2] = {0, 0}, haltUs_[2] = {0, 0};
    // The acknowledged word write of the active route, for WORD67's 0007 to follow its own 0006.
    uint32_t writtenOperation_ = 0;
    uint64_t writtenUs_ = 0;
    // Earliest bound of the latest fresh read's 6064 and 606C: a relative witness's observation time.
    uint64_t feedbackUs_ = 0;
    // Each node's latest acknowledged 607A download of this object, for TRIAL_BASE; never authority.
    int32_t commanded_[2] = {0, 0};
    uint32_t commandedTrial_[2] = {0, 0}, commandedEpoch_[2] = {0, 0};
    bool commandedKnown_[2] = {false, false}, commandedRelative_[2] = {false, false};
    // Motion admissions per motion route and node; read routes, READ_HISTORY included, have none.
    uint8_t motionTotal_ = 0, motionCases_[unsigned(RouteKind::HOME) + 1][2] = {};
    // An arming route: a successful terminal whose reads context_ and identity_ still hold.
    bool armed_ = false;
    RoutePlan armedPlan_;
    int32_t armedPosition_ = 0;
    uint64_t armedFeedbackUs_ = 0;
    // Attended admissions without motion weight per route (SETTINGS_2202, IO_DI3, HOME, VELOCITY_TARGET)
    // and node.
    uint8_t settingsCases_[4][2] = {};
    // MOVE with an observer: node B's reads, apart from the route's own on node A.
    bool observerPending_ = false, observerActive_ = false;
    MotorControlCAN::CANOPEN::SdoTransfer observerExpected_;
    uint64_t observerNextUs_ = 0, observerCycleUs_ = 0;
    uint32_t observerGrantOperation_ = 0, observerGrantStep_ = 0;
    RouteWork* work_ = nullptr;
    bool settingsSettled_ = false; ///< The transaction's result is recorded and the session followed.
    bool ownerFollowed_ = false;   ///< The owner's reservation already moved to the new epoch.
    uint8_t stepIndex_ = 0, subStep_ = 0, readMode_ = 0, readArg_ = 0;
    bool occupied_ = false, pending_ = false, stepActive_ = false, readActive_ = false;
    bool singleAdmitted_ = false, actionStarted_ = false, abortRequested_ = false, emcyHandled_ = false;
    bool lossHandled_ = false;   ///< Lost evidence has decided the end and withdrawn active work.
    /// MOVE: the planned stop was asked for, whatever its path. VELOCITY: the route's own fallback
    /// QUICK_STOP after a run that ended needing a stop with none sent (backlog S16).
    bool stopAttempted_ = false;
    bool modeAdmitted_[2] = {false, false};
    RouteFailure endFailure_ = RouteFailure::NONE; ///< Abort or EMCY, applied once work settles.
    uint32_t emcyBaseline_[2] = {0, 0}; ///< Each node's emcyCount when this route last handled it.
    const char* endWhy_ = "none";

    bool continueOperations(AxisSession& session, uint8_t slot, uint64_t nowUs) noexcept;
    uint32_t nextGrant(uint8_t slot) const noexcept;
    AxisSession& axis() noexcept { return *sessions_[record_.plan.node - 1]; }
    const AxisSession& axis() const noexcept { return *sessions_[record_.plan.node - 1]; }
    uint8_t slot() const noexcept { return uint8_t(record_.plan.node - 1); }
    bool currentTarget(MotorControlCAN::LICHUAN_C::ReadTarget& output) const noexcept;
    void finish(RouteFailure failure, const char* why, uint64_t nowUs) noexcept;
    void endAfterSettling(RouteFailure failure, const char* why) noexcept;
    uint64_t withdrawalTime(uint64_t nowUs) const noexcept;
    void capture(bool tail) noexcept;
    void lose(uint32_t count, bool tail) noexcept;
    void copyTerminal(const MotorControlCANOwner::Completion& done) noexcept;
    void copyTraffic(const MotorControlCANOwner::Diagnostic& in, bool tail) noexcept;
    bool settle(uint64_t nowUs) noexcept;
    void advanceStep(uint64_t nowUs) noexcept;
    void nextStep(uint64_t nowUs) noexcept;
    bool startRead(uint8_t mode, uint8_t arg, uint64_t nowUs) noexcept;
    void serviceRead(uint64_t nowUs) noexcept;
    void readDone(uint64_t nowUs) noexcept;
    bool snapshot(uint8_t mask, uint64_t nowUs) noexcept;
    void admit(const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t nowUs) noexcept;
    bool startWrite(uint64_t nowUs) noexcept;
    void serviceWrite(uint64_t nowUs) noexcept;
    bool writeTransfer(MotorControlCAN::CANOPEN::SdoTransfer& output) const noexcept;
    const char* predecessor(uint8_t write, uint8_t word = 0) const noexcept;
    const char* checkMode() const noexcept;
    const char* checkModeOneReleased() const noexcept;
    const char* checkModeOneCycle(const RouteSnapshot& snapshot) const noexcept;
    const char* checkIdle(const RouteSnapshot& snapshot) const noexcept;
    const char* checkPosition() const noexcept;
    const char* checkRearm() const noexcept;
    const char* checkMove() const noexcept;
    const char* checkReleased() const noexcept;
    const char* checkWitness() const noexcept;
    const char* checkModeThree() const noexcept;
    const char* checkVelocity() const noexcept;
    const char* checkTargetState(const RouteSnapshot& snapshot) const noexcept;
    const char* checkVelocityTarget() const noexcept;
    const char* checkTargetAfter() const noexcept;
    void startVelocity(uint64_t nowUs) noexcept;
    void startHome(uint64_t nowUs) noexcept;
    bool rearmable(uint8_t slot, const AxisSession& session, uint8_t predecessor) const noexcept;
    void noteEnd(uint64_t nowUs) noexcept;
    bool noteSettings(MotorControlCANOwner::SequenceId sequence, const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                      uint64_t nowUs) noexcept;
    void startSettings(uint64_t nowUs) noexcept;
    void serviceSettings(uint64_t nowUs) noexcept;
    bool settleSettings(uint64_t nowUs) noexcept;
    bool followConfiguration(uint64_t nowUs) noexcept;
    void publishGroup(unsigned which, const MotorControlCAN::LICHUAN_C::DriverContext* read, uint64_t nowUs) noexcept;
    AxisSession& observerAxis() noexcept { return *sessions_[record_.plan.observer - 1]; }
    bool startObserverRead(uint8_t mask, uint64_t nowUs) noexcept;
    void serviceObserverRead(uint64_t nowUs) noexcept;
    void observerReadDone(uint64_t nowUs) noexcept;
    void serviceObserver(uint64_t nowUs) noexcept;
    void startClear(uint64_t nowUs) noexcept;
    void startEnable(uint64_t nowUs) noexcept;
    void startMove(uint64_t nowUs) noexcept;
    void startStop(uint8_t kind, uint32_t deadlineMs, uint32_t bestEffortMs, uint32_t intervalMs,
                   const MotorControlCAN::LICHUAN_C::ReadContext& state, uint64_t nowUs) noexcept;
    void serviceMove(uint64_t nowUs) noexcept;
    void serviceAction(uint64_t nowUs) noexcept;
    void serviceObserve(uint64_t nowUs) noexcept;
    void observeDone(uint64_t nowUs) noexcept;
    unsigned summaryCount() const noexcept;
    bool formatSummary(unsigned index, char* output, size_t size) const noexcept;
};
} // namespace MotorControlCANExample
