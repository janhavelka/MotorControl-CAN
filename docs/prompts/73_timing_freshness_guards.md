# Prompt 73 Simple timing: service timeouts, deadlines and freshness as optional guards

Apply [COMMON.md](COMMON.md). Prerequisites: [17](17_pdo_observations.md), [42](42_cli_motion_routes.md), [52](52_motion_changes.md), [53](53_multi_axis.md), [56](56_host_throughput.md).

## Objective and boundaries

The user's words, verbatim (2026-11-17), on the fixed console move lifetime and the replacement window: "Propose
logical, good engineering proposal how to address this - or to remove this "rule" completely? We need functional,
simple to use library not an overcomplicated, overrestrictive "you can not do anything" repository - this regards to
the freshness too - there are tools for that like the PDO and others - i would give the end user more responsibility
and these extra tools as optional, settable guards"; on the per-service budgets: "dont understand, but design better
industry standard/good engineering practice, scalable, simple and functional solution"; and generally: "In general we
are after good engineering practises and more responsibility on the end user of this firmware with possible optional
settable guards".

Facts (code at the time of writing; recheck before changing):

- Every console move gets a fixed 10 s lifetime (`examples/common/ProbeConsoleMotion.cpp`, `tail = 10000000`, "An
  application deadline, not a manufacturer completion delay"). A slow or long move ends at that deadline.
- A move replacement needs `MOVE_SERVICE_STEPS + 8` = 21 services left before the original deadline
  (`src/profiles/lichuan_c/Position.cpp`, "replacement services do not fit the original deadline"); at the bench's
  budgets that is 7.35 s, so changes fit only in the first 2.6 s of a 10 s move (bench 2026-11-16).
- Per-service budgets are host policy: queue 100 ms and response 250 ms (`examples/s2_bench/BenchRoute.h`,
  `ROUTE_QUEUE_US`, `ROUTE_RESPONSE_US`; `ReadOptions::queueTimeoutUs`/`responseTimeoutUs`, "Nonzero caller policy, not
  a vendor delay"). The drive's own SDO reply is under about 2.3 ms; one read takes about 7 ms only because of today's
  pass pacing ([throughput report](../reports/2026-11-12_host_throughput.md)).
- Every age cap is a multiple of those budgets: 9× (3.15 s) for move and velocity prerequisites and the console
  arming, 7× (2.45 s) for stop state, 8× for the velocity stop reserve (`examples/s2_bench/BenchConsole.cpp`), and the
  library requires a move's `maximumAgeUs` to cover seven fields and a stop's five (`Position.h`, `Actions.h`). The
  bench's two-axis trials needed a paired arming to fit 3.15 s, and HM3's second stop fell to the unconfirmed
  best-effort path because its state was older than 2.45 s ([HIL report](../reports/2026-11-16_hil_campaign.md)).
  Since `db4e5f3e` (2026-11-17) `AxisSession::requestStop` uses the freshest complete report it holds (the caller's,
  the latest motion cycle, or the other stop kind's last cycle), so a stop shortly after a HALT is sequenced; a stop
  more than 2.45 s after every held report still is not.
- State for admissions and stops comes from repeated SDO read cycles. TPDO samples are not used for decisions yet
  (D11 in the [backlog](../backlog.md); [throughput report](../reports/2026-11-12_host_throughput.md), "PDO samples
  are not yet qualified"). CANopen clients take the SDO timeout as a caller parameter (CANopenNode
  `CO_SDOclientUploadInitiate(..., SDOtimeoutTime_ms, ...)`), and the pinned copy is in
  `experiments/canopen_service_fit/vendor/CANopenNode/301/CO_SDOclient.c`.

### Design proposal

1. **One per-node service timeout.** Each node binding carries a `responseTimeoutUs` (how long the client waits for
   one SDO reply) with a documented project default chosen from prompt 56's measured reply times and a validated range.
   The separate queue budget goes away: time spent waiting in the host queue counts against the operation's own
   deadline. All derived service counts use this value, so faster settings shorten every derived bound.
2. **Deadlines the user sets, with sane defaults.** An operation's deadline is the application's choice. The library
   keeps accepting an explicit deadline and also accepts "no software deadline" for moves and homing, which then end on
   completion, a stop, a fault or loss of the node. The console computes its default from the move itself (distance at
   speed plus both ramps, times a settable factor, plus a settable margin) and takes `deadline MS|none` on the line.
   The fixed 10 s lifetime is removed. The replacement window is "fits the remaining deadline, if one exists", counted
   at the configured timeout.
3. **Freshness as an optional guard.** `maximumAgeUs` becomes a settable guard with a documented default per operation
   kind and an explicit "off". What stays mandatory is causal ordering, not age: a report that judges a command must
   have been acquired after that command was acknowledged, and one cycle's blocks must belong to one cycle. A refusal
   names the guard and its value.
4. **Freshness from the bus, not from polling.** When a TPDO stream with a confirmed actual map carries the state words
   (`6041`, `2100`, `6061`, `6064`, `606C`), the session's latest complete report comes from it (event timer or SYNC),
   so admissions and stops have a fresh state without an SDO cycle. The guard is the sample's maximum age. Where
   AGENTS.md's PDO route-retirement rule (D11) leaves a sample unqualified, the session falls back to SDO reads.
   Heartbeat consumer time is the node's liveness, separate from state freshness.
5. **Kept invariants.** Motion starts, enable, homing, save and NMT are never replayed; a stop is never refused for a
   guard or a cap and keeps priority; uncertainty after an unconfirmed write is reported; a best-effort stop stays
   labelled unconfirmed. Defaults live in one documented place and are reported in health and results.

Boundaries: guards are application settings reachable through the library API; examples and the console only call
that API. No hidden retry, clock or I/O enters the portable core.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its timeout, deadline and freshness seams; RS is
authoritative for common names. Inspect FieldCore-node read-only for its per-device response timeout and command
deadline settings ([audit](../reports/2026-11-17_fieldcore_rs485_audit.md)). If HEAD, dirty state and seam match the
last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or
rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read
`Reads.h`, `Position.h`/`.cpp`, `Velocity.h`/`.cpp`, `Actions.h`/`.cpp`, `AxisSession`, `ProbeMotion`,
`ProbeConsoleMotion.cpp`, `BenchRoute.h`, `BenchConsole.cpp`, the prompt 17, 51, 52 and 53 reports, prompt 56's
report and measured reply times, and backlog D11.
Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Inventory every timing rule in the library, the examples, the bench, the console and the Python runners: value,
   owner, purpose, and whether it is a causal rule or a policy guard. Record it in the report; it drives the changes.
2. The per-node response timeout in the binding and `ReadOptions`/service options, with its default and range; remove
   the separate queue budget; derive service counts from it.
3. Optional deadlines: "no software deadline" for moves and homing in the library, the console's derived default with
   `deadline MS|none`, and the replacement window counted against the remaining deadline only.
4. `maximumAgeUs` as an optional guard with per-kind defaults and "off"; keep the causal ordering checks; every refusal
   names its guard.
5. TPDO-fed state: the session and `ProbeMotion` take a qualified TPDO sample as the latest complete report for
   admission, observation and stop naming, with a settable sample age; SDO fallback where the sample is unqualified.
6. Bench and console: route and arming budgets from the per-node timeout; the arming age becomes a settable guard (the
   paired arming of route protocol 16 stays available); a `timing` console command that reads and sets the guards
   through the API.

## Verify and exit

Host tests: a move with no software deadline runs to completion and still stops on request; the console's derived
deadline for slow and long moves; the replacement window at short timeouts; each guard on and off, with its refusal
naming it; causal ordering still refuses a report older than its command; a TPDO-fed stop after a stale SDO cycle is
sequenced; an unqualified TPDO sample falls back to SDO; no replay anywhere. Run the native, firmware, stack and
documentation gates and CI on the actual final commit. Bench: a 30 s console move at low speed, a replacement late in
a move, the two-axis trials without the paired arming, a stop 5 s after a HALT, then a short regression.

## Documentation and delegation

Update the axis contract, velocity, position and stop guides, the CLI contract, the owner guide, the bench README,
coverage, backlog (D11 where it changes), CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the
causal-ordering rules and the TPDO-fed state; normal coding agents own disjoint tests. The lead integrates and runs
combined checks. Record `docs/reports/prompt_73.md`. Commit and sync on the current branch before handoff.
