# Prompt 77 Switch-ready limits and homing: input facts, limit admission, soft-limit state and deferred switch trials

Apply [COMMON.md](COMMON.md). Prerequisites: [21](21_io_settings.md), [36](36_homing.md), [42](42_cli_motion_routes.md).

## Objective and boundaries

The owner's request, verbatim (2026-11-19): "for homing and limits - get inspired with ongoing implementation of
motorcontrol.rs which is doing exactly this right now and will be implementing for next few days - you could do similar
work fitted to the CAN drives and let me afterwards attach some real switches and similar - so for all - do the prep at
least". The owner is away from 2026-11-20 to about 2026-12-04 and nobody can touch the bench. The goal is a
**switch-ready** library and bench: everything that can be built, host-tested and run unattended now, with every trial
that needs a switch, a power cycle or an observation written as a deferred owner step. The owner also directed on
2026-11-19 that work stays on important, high-gain items.

Facts at the time of writing (recheck each against the cited page or record before use):

- **Inputs.** DI codes 0..13 (0 undefined, 1 origin, 2/3 positive/negative limit, 4 enable, 5 stop, 6 emergency stop,
  7..13 user 0..6) and `2400:01` polarity bits 0..6: EN86/ZH86 p8 and p18/p19 ([I/O guide](../io_settings.md)).
  `2101` bits 0..6 report the **effective** level, after polarity (EN86 p17, ZH86 p18). DIn on p8 and Xn on p17/p18
  share the subindex. `60FD` has no documented layout. Wiring (EN86 p7 notes): DI signals are 24 V, a current-limiting
  resistor above that; with DICOM at 24 V a DI pulled to 0 V is effective, with DICOM at 0 V a DI at 24 V is.
- **Specimens.** Both power up with DI0..DI2 = 1, 2, 3, polarity 0, DI3..DI6 = 0, `2101` = 0; the terminals are
  reported unconnected ([bench record](../hardware_bench.md), [homing guide](../homing.md#bench-observations-2026-11-09)).
  On node 1, DI3 took 0 to 7 to 0, and polarity bit 3 raised `2101` bit 3 on the open terminal while `60FD` stayed 0;
  the polarity read 0 after the next power cycle with no save ([I/O guide](../io_settings.md#specimen-readings-2026-11-04)).
  Every motion run since, both directions included, ran with DI1/DI2 assigned limits at polarity 0 on open terminals.
  That is consistent with an open polarity-0 terminal not asserting its function; it is an inference, not a limit test.
- **Drive reaction.** No retained source states what the drive does when a limit, stop or emergency-stop input
  becomes effective outside homing: direction, stop ramp, alarm, `603F`/EMCY code, status bits or recovery (EN86/ZH86
  p8 give labels only). `2100` bits 5/6 are software-limit status (EN86 p17, ZH86 p18); no bit reports a hardware limit.
- **Homing.** Methods 17, 18, 24 and 29 only (EN86/ZH86 p13-15). No current-position or index method, withdrawal
  distance or search timeout is documented; starting on an active switch and meeting a limit during an origin search
  are undocumented. The library refuses at its CLOSED gate (`code=3 detail=7`) and Q04, Q11, Q14 and Q21 stay open
  ([homing guide](../homing.md), [device conflicts](../device_conflicts.md)). **No homing method can run without a switch.**
- **Soft limits.** `2202` 0 noneffective, 1 "effective after homing" (EN86/ZH86 p18), typed write of 0 and 1;
  `607D:01/:02` I32 without domain or coordinate basis, no write (Q05); `607D:00` answers in two bytes (Q01)
  ([driver settings](../driver_settings.md#setting-decisions)). Drive soft limits therefore wait for a qualified home.
  Position, velocity and homing treat `2100` bits 5/6 as an unclean report today (`clean` in `Position.cpp`,
  `Velocity.cpp`, `Homing.cpp`). Host soft limits exist: `AxisConfig::softLimitsKnown`, `softMinimum`, `softMaximum`,
  inclusive absolute command coordinates, checked by `preparePosition` ([axis contract](../axis_contract.md));
  velocity has no host limit.

Owner-absence rules, adopted from RS decision D15 and binding until the owner's return is recorded:

1. **No waiting.** Ask nothing. A step that needs the owner goes into the deferred table below with the exact action
   and the cases it unlocks; those cases are NOT RUN, "deferred: owner away". The prompt stays Partial.
2. **RAM only.** No `2300` save or factory restore, no `2203`/node, bitrate or PDO-map write; the forbidden and
   allowed lists are [COMMON](COMMON.md#owner-absence-20-november-to-about-4-december-2026) rules 2 and 3.
3. **No state only a power cycle clears.** No deliberate FF05 or enable-fault (S21) reproduction. A drive that still
   ends in such a state is stopped if needed, recorded and left out of later cases.
4. **Snapshot discipline.** Take a fresh `2400:00..08` snapshot before any input write and restore it on the same node
   before selecting the other node, flashing or ending the session; read back the restore.
5. **Input writes unattended** only on DI3..DI6, one field per candidate: a function code 0, 1, 2, 3 or 7..13 at
   polarity 0, or a polarity bit on a terminal whose function is 0. Never write DI0..DI2, never assign 4, 5 or 6, never
   leave a terminal with a function 1..13 and its polarity bit set, even transiently. Any change in `2100`, `6041`,
   `603F` or an EMCY after an input write ends the series on that node.

Boundaries: CL86-C over CANopen only; common names follow RS where the meaning matches, CANopen and Lichuan names stay
native. No startup, probe or recovery path changes an input, enables, homes or moves. An unconnected terminal does not
disable its function. Physical observations stay "not observed". Nothing here is safety-rated. MCU-mapped switches stay
with [prompt 69](69_application_io_watchdog.md), and the stop and emergency-stop line with
[prompt 62](62_stop_line.md); this prompt shares their drive-side input type and does not run their trials.

## Read and audit

Refresh MotorControl-RS HEAD and dirty state and read its limit, input and homing seams; RS is authoritative for common
names. At writing RS HEAD is `0412698` with uncommitted work in 45 files, and this seam is changing daily: reread the
named commits (`7f95e46`, `852ea51`, `05c02d4`, `b977ea0`, `dd2fb1c`, `bae5048`, `e4d0e3d`, `93f8f36`, `29ca5bf`) and
anything newer. Inspect FieldCore-node read-only for input projections and limit ownership. If HEAD, dirty state and
seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a
behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or
verification rules. Starting dispositions, to verify:

- **Adopt (common meaning matches):** `InputWiring` UNKNOWN, UNCONNECTED, CONNECTED as a caller declaration
  (`include/MotorControlRS/ReadOperation.h`); the outcome names `LIMIT_INPUT`, `STOP_INPUT`, `SOFT_LIMIT`,
  `STOPPED_SHORT` (move) and `STOPPED_EARLY` (velocity), each a reported fact, never completion and not a proven cause
  (`MoveError`, `VelocityError` in `MoveOperation.h`, `VelocityOperation.h`); witness masks built only from terminals
  declared CONNECTED (`limitInputMask`, `stopInputMask` in `profiles/ess_rs/Position.h`); refusal of every start while
  an assigned limit reads active, stale or unknown, in both directions until an away start is measured, and stops never
  refused (`limitInputs` in `examples/probe_cli/MotionReadinessApp.h`); the soft-limit state UNKNOWN, KNOWN_DISABLED,
  BOUND per target, binding and generation, with UNKNOWN refusing starts and a home needing KNOWN_DISABLED
  (`SoftLimitState` in `examples/probe_cli/SoftLimitApp.h`); host soft limits installed only after a confirmed
  readback; input facts before a write on a CONNECTED terminal (`input_facts_required`, `input_active`, `7f95e46`); the
  per-method input rule shape of "Input levels during home" (`docs/ess_homing.md`); D15
  (`docs/prompts/ess_wired_bench/COMMON.md`); baseline capture and per-field restoration (`scripts/bench_records.py`).
- **Reject (ESS or Modbus specific):** function numbers above 3 (ESS 4 is release, 7..13 motion triggers, 14..17
  segment selectors); `IoStatusMapping`, since `2101` is documented as the effective level; `0x0017`, `0x0018`, the
  pair words and word order; ESS measured reactions and timings (limits stop toward only with no alarm or recovery,
  stop 108-114 ms, emergency stop 20-23 ms, 10 ms pulses, filter 2.5 ms per unit) on model `0x4EEA` firmware `0x0029`;
  methods 1-35, method 35 and auxiliary 0-11 with the `soft-limits` home fact; relay and MOSFET host-driven switches
  and the D14 line-isolation interlock, which need bench outputs this fixture lacks; and keeping limit and soft-limit
  state only in the example application, because a CLI must not be the only place a motor feature lives (AGENTS).

Read the prompt 21, 36, 62 and 69 prompts and reports ([21](../reports/prompt_21.md), [36](../reports/prompt_36.md),
[audit](../reports/prompts_20_21_audit.md)), the [I/O](../io_settings.md), [homing](../homing.md),
[driver settings](../driver_settings.md), [stop](../stop_actions.md) and [velocity](../velocity.md) guides, the ledger
rows `2400`, `2101`, `60FD`, `2100`, `2202`, `607C`, `607D`, `6098`, `6099`, `609A` in the
[reviewed ledger](../reference/lichuan_review.md) and [motion facts](../audit/LICHUAN_MOTION_FACTS.md), and the
`IoSettings.h`, `Settings.h`, `Homing.h`, `Position.h`, `Velocity.h` and `AxisSession` code. Reopen the original
[EN86](../reference/vendor/86C_CANopen_user_manual_en.pdf) and [ZH86](../reference/vendor/86C_CANopen_user_manual_zh.pdf)
pages 7-8, 13-15 and 17-19 for every value used. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

Library first; the console and bench only call it.

1. **Input facts.** A per-axis drive-input description: the decoded assignment from a fresh `IO_INPUT_CONFIGURATION`
   read (origin, positive limit, negative limit, stop, emergency-stop terminals), the application's `InputWiring`
   per terminal, and a typed checked effective-state observation from `2101` with its age; `60FD` stays raw. One type
   shared with prompt 62's stop-input description (if 62 has not run, create it here and 62 consumes it).
2. **Limit admission** in the profile's move, velocity and home preparation (installed library), with the example
   `AxisSession` and group stop supplying the input evidence: an assigned limit that reads active, a stale `2101` or
   an undecodable assignment refuses before TX with the terminal named. Both directions refuse until two facts are
   measured: which native sign runs toward each limit (unknown; Q03, Q04, `2200`) and that the drive accepts a start
   away from a held limit. Then the rule becomes direction-aware:
   toward refuses, away admits; an absolute move without a witness has no direction and still refuses. The guard is on
   by default and settable per axis; an admitted result names a guard that was off. Stops are never refused.
3. **Observation and outcomes.** When a CONNECTED limit or stop terminal is declared, each motion cycle carries `2101`
   (measure the added cycle cost). A move or velocity run that stops before arrival with a masked input effective ends
   `LIMIT_INPUT` or `STOP_INPUT`; without one, `STOPPED_SHORT` or `STOPPED_EARLY`. `2100` bits 5/6 become `SOFT_LIMIT`,
   split from alarm and release, keeping `needsStop` until a checked stop; homing keeps them as faults. Place the names
   where CONVENTIONS section 3 puts terminal reasons and record the placement there.
4. **Soft-limit state** per axis from fresh SOFT_LIMITS and state reads: KNOWN_DISABLED (`2202` = 0), BOUND (`2202` = 1
   written by this application after a qualified home, read back, with host limits installed from a `607D` domain),
   UNKNOWN otherwise (code 10, a lost acknowledgement, reboot, rebinding, counter clear, release). UNKNOWN refuses
   starts; a home needs KNOWN_DISABLED. BOUND stays unreachable until Q05 and homing close; implement and test it, do
   not open it.
5. **Host travel guard for velocity** (optional, settable, off by default): with host soft limits known, refuse a run
   whose direction leaves the limits from a witness at or beyond them, and end a run whose fresh `6064` crosses them
   with the run's own stop. Report the overshoot bound as speed times the cycle interval, a host latency, not a drive
   guarantee. Name its outcome distinctly from the drive's `SOFT_LIMIT`.
6. **Homing input rules** for 17, 18, 24 and 29 in `prepareHome` and each poll: the method's switch on a terminal
   declared CONNECTED (UNKNOWN or UNCONNECTED refuses with `HomeError::INPUT`), its switch inactive at admission,
   other limit, stop and emergency-stop inputs inactive, method 29's active-to-inactive origin transition observed.
   The closed qualification gate stays closed.
7. **Snapshot and restore** of `2400:01..08` per node as a library call over `prepareIoSettings`: restore writes one
   differing field per candidate in an order that never leaves rule 5's forbidden combination; the retained snapshot
   carries target, generation and time, and a restore refuses on another target.
8. **Console and bench.** `io inputs` (assignment, wiring, `2101`, `60FD`, ages), `io wiring DI STATE`, `io snapshot`,
   `io restore`, single-field `io di` and `io polarity` writes under rule 5, `limits status`, host-limit edits through
   the existing `axis config` path, and `home check METHOD` (admission only, no TX). New bench routes follow the `T`
   line pattern with a protocol bump, records and runner support; the runner checks the snapshot before and after.
9. **Unattended bench now**, both nodes, after a software check of fixture and state (the drives may hold latches from
   earlier sessions; read, do not assume):
   - B1 snapshot: identity, `IO_INPUT_CONFIGURATION`, `MONITORS`, `INPUTS`, `DriverGroup::HOMING`, SOFT_LIMITS (under
     `CountWidths::MEASURED_U16`); compare with the 2026-11-04 and 2026-11-09 records.
   - B2 input writes under rule 5 on DI3..DI6 with readback and restore; for each polarity change poll `2101` and
     `60FD` at a fixed interval to bound when the change takes effect; repeat the 2026-11-04 DI3 cases on node 2.
   - B3 admission without motion: limit rules with the actual wiring declarations, `home check` for all four methods
     (expected refusals named), the soft-limit state from the live `2202`.
   - B4 host soft limits, only under the owner's written unattended waiver for this period that covers motion
     (`--unattended-waiver`, `--waiver-covers-motion`, [bench record](../hardware_bench.md)), at most 60 rpm and a few
     seconds with the route's own stop: a short absolute move inside the limits, an endpoint outside refused with zero
     TX, an inward move from outside admitted, the velocity guard's refusal and one crossing stop. Without such a
     waiver, B4 is deferred. End with both nodes stopped and the B1 snapshot re-read equal.
10. **Deferred to owner return** (add as a table in the report and the [backlog](../backlog.md)):
    - O1 Power-cycle both drives; take B1 again as the post-absence baseline.
    - O2 Emulated switches without wiring, on one node with a power cycle available: at standstill, set the polarity
      of DI1, then DI2, then DI0 on the open terminal and record `2101`, `60FD`, `2100`, `6041`, `603F` and EMCY; then
      a slow velocity run with the limit effective (toward and away), the run with the limit made effective mid-run,
      recovery after restoring polarity. This answers the reaction, the sign-to-limit mapping and away starts first.
    - O3 Wire switches per EN86 p7 (24 V, DICOM choice recorded, switch to the effective level): origin on DI0, positive
      limit on DI1, negative limit on DI2, per drive; record actual pins and switch type in the bench record. The owner
      chooses normally open at polarity 0 (no save, a broken wire is not detected) or normally closed with the polarity
      bit set (a broken wire asserts the limit, but the volatile polarity then needs an explicit write after each power
      up or an owner-chosen save). A pushbutton is enough for manual trials; an MCU-driven opto or relay per input
      (prompt 69's outputs) makes them repeatable.
    - O4 Trials then unlocked: polarity check per switch; limit stop toward during a move and a velocity run, each
      direction; away start with a held limit; behavior after release; homing 17, 18, 24, 29 from each admitted start,
      with direction against `2200` (Q04), completion dialect and `607C` effect (Q14); then `2202` = 1 with a `607D`
      window and its bits 5/6 reaction (Q05), restored to 0. Prompt 62's stop-line trials can use the same session.

## Verify and exit

Host tests, adversarial: every function code and polarity per terminal with each wiring declaration; a limit active,
stale, undecodable or on an UNKNOWN terminal refusing moves, velocity, homes and group members with zero TX; stops
admitted throughout; the direction rule closed until both facts exist, and open only with them; a move ending
`LIMIT_INPUT` versus `STOPPED_SHORT` versus arrival; `SOFT_LIMIT` from bits 5/6 keeping `needsStop` and never completing;
soft-limit state transitions on every invalidation; the velocity guard's refusal, crossing stop and off state; homing
input rules per method on admission and polls, method 29 without the transition; snapshot and restore across a lost
reply, an abort, a readback mismatch and another target, and the forbidden combination refused before TX. Run the
native, firmware (S2, S3, Arduino), stack and documentation gates and CI on the actual final commit. Bench: B1 to B4 as
allowed, each with its record and the restore read back. Exit **Partial** with the deferred table; the switch trials
are owned work, not dropped.

## Documentation and delegation

Update the I/O, homing, driver settings, stop, velocity and axis guides, the CLI contract, CONVENTIONS (new names),
coverage, backlog, `hardware_bench.md` (wiring section and the deferred table), device conflicts for any new
observation, CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the admission, direction and
soft-limit rules and rule 5; normal coding agents own disjoint tests, console and runner changes. The lead integrates,
runs combined checks and alone controls the port. Record `docs/reports/prompt_77.md` with the RS rows. Commit and sync
on the current branch before handoff.

## Appendix: optional instrument prep

Software only, with host tests; no driver installs and no vendor tool run.

- **PC listen-only CAN observer** (PCAN-USB; backlog H04, Q18, [prompt 47](47_hardware_finite.md)): an offline importer
  for the observer's exported trace, written from that format's published specification, and a comparison with the
  S2 records (`s2_evidence.py`) that answers Q18's questions offline (reply order after a slow write, a request while
  one is pending, duplicate delivery) and lists what the S2's own capture cannot see; a capture plan the owner can run.
- **Logic analyzer on the S2 console UART TX (GPIO43)** for H04 console loss: `scripts/s2_uart_compare.py` exists;
  add an importer from the analyzer's decoded-UART export to its raw-plus-times format, with tests, and take the line
  rate from the shared console rate constant (prompt 75), not a fixed 115200.
