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
- The state fields do not fit one PDO. The [EDS](../reference/vendor/LC_StepDriverCAN_V1.1.eds) declares `6041` and
  `2100` U16, `6061` U8, `6064` and `606C` I32 (13 bytes), and each `1A00..1A03:00` allows at most four entries; a
  Classical CAN PDO carries eight bytes. The EDS default maps are TPDO1 `6041`/`60FD`/`6061` and TPDO2 `6064`/`606C`,
  but on 2026-11-06 both specimens read TPDO1 `6041`/`6061`/`6064` with event timer 0 and TPDO2..4 empty
  ([PDO stream report](../reports/2026-11-06_pdo_stream.md)), so no live TPDO carries `2100` or `606C`. TPDOs carry
  no cycle tag, and same-SYNC sample identity (Q27) is open. `prepareStop` decides on `2100`, `6041` and `6061`; moves
  and some stops add `6064` and `606C`, and homing adds `2101` (`StateBlock` in `Reads.h`).
- Every service cutoff is finite today: `SdoClient::checkShape` refuses a zero `queueTimeoutUs` or
  `responseTimeoutUs`, the queue and TX-handoff cutoffs are the earlier of the deadline and `now + queueTimeoutUs`,
  and the reply cutoff is the earlier of the deadline and `responseTimeoutUs` after handoff (`cutoff` in
  `CanBusOwner.cpp` and `SdoClient.cpp`). Homing also ends at `HomeOptions::maxPolls` (`OBSERVATION_SCOPE`), and a
  move ends `ITERATION_EXHAUSTED` at its last step identity.
- Some stop refusals are not guards. `CanBusOwner::admitBestEffortStop` refuses without transport, with an inhibited
  controller, a blocked route or NMT state, an invalid channel, exhausted request identities or an occupied urgent
  slot; `prepareStop` refuses HALT after `000B`, after `010F` or in a released state, because `010F` carries the
  enable-operation bits (prompt 31; a CiA402 inference, not a measurement). A stop waits for in-flight work on its
  channel to settle and never cancels it (`AxisSession`).
- Native moves may name command increments with no declared scale, and an absolute target is not a displacement
  without a current position (`conversionConfig` and `preparePosition` in `src/Axis.cpp`). The ramp times' meaning,
  rest to the commanded speed, is a documented figure fitted on four runs and still open as Q11
  ([speed units](../speed_units.md)).

### Design proposal

1. **One per-node service timeout; every service attempt stays finite.** Each node binding carries a
   `responseTimeoutUs` (how long the client waits for one SDO reply) with a documented project default chosen from
   prompt 56's measured reply times and a validated range. The separate queue setting goes away, but not the queue
   cutoff: each yielded service's queue, TX-handoff and reply cutoffs are derived from this one timeout by one
   documented rule, stay finite whether or not the operation has a deadline, and give way to an earlier operation
   deadline. An attempt that expires ends with its own outcome and keeps its effect evidence (a write that may have
   left stays uncertain and is not replayed), the operation ends as it does today on a failed service, and other nodes
   stay serviceable. Expiry follows CONVENTIONS' deadline cutoff (`now >= deadlineUs` expires even if the frame was received earlier), queue time consumes the operation's
   deadline, and expiry is `now >= deadline` (guide rule C10, [communication practice](../communication_practice.md)).
   All derived service counts use this value, so faster settings shorten every derived bound.
2. **Deadlines the user sets, with sane defaults.** An operation's deadline is the application's choice. The library
   keeps accepting an explicit deadline and also accepts "no software deadline" for moves and homing. That removes
   only the overall cutoff: such an operation still ends on completion, a stop, a fault, loss of the node, a failed
   service attempt (item 1), its last step identity (`ITERATION_EXHAUSTED`) or the owner's last request identity
   (`IDS_EXHAUSTED`), never by wrapping a counter; homing's `maxPolls` becomes optional with it. "None" is an explicit
   state of the operation and its owner sequence and is never passed down as `UINT64_MAX` service cutoffs. The
   console computes its default from the move itself (distance at speed plus both ramps, times a settable factor, plus
   a settable margin) and takes `deadline MS|none` on the line. The fixed 10 s lifetime is removed. The replacement
   window is "fits the remaining deadline, if one exists", counted at the configured timeout; a replacement keeps the
   original's immutable deadline.

   The console's estimate is host policy, not a vendor completion time, and exists only when its inputs do: a native
   displacement (relative: the prepared quantized displacement; absolute: the target minus `6064` from a complete
   report of this axis in the same coordinate basis, never an assumed zero), a declared command scale (never an
   invented subdivision), a nonzero native speed, and both ramp times read as rest to the commanded speed (Q11,
   inferred). It uses the displacement's magnitude, so reverse travel is estimated like forward travel; zero
   displacement uses the same formula; checked arithmetic refuses an overflowing or out-of-range estimate. Without an
   estimate the console refuses only that line, names the missing input and asks for `deadline MS|none`; the library
   never refuses an otherwise valid native move for lacking one. Replies and results say whether the deadline was
   estimated, explicit or none.
3. **Freshness as an optional guard.** `maximumAgeUs` becomes a settable guard with a documented default per operation
   kind and an explicit "off". What stays mandatory is causal ordering, not age: a report that judges a command must
   have been acquired after that command was acknowledged, and one cycle's blocks must belong to one cycle; a report from a route reopened under prompt 55's accepted
   risk meets this as accepted, not established, and carries the mark. A refusal
   names the guard and its value.
4. **Freshness from the bus, not from polling.** TPDO samples (event timer or SYNC) replace an SDO cycle only for the
   fields a decision needs, and only from routes whose actual map the application configured explicitly and confirmed
   ([prompt 18](18_pdo_configuration.md)'s typed sequences; never an implicit remap) and whose samples prompt 17's
   qualification accepts. Each decision has a recorded minimum field set (stop naming on the TPDO path: `2100`, `6041`,
   `6061`, five bytes in three entries, narrower than the SDO STATUS and MONITORS blocks `prepareStop` takes today, so
   `prepareStop` gains a TPDO-report input); prefer one reviewed TPDO that carries a decision's whole set. Where a set spans TPDOs, the
   report is assembled per field and is complete only when every required field comes from a current qualified route
   and node generation and was captured after the acknowledgement of the command it judges. Its acquisition window
   runs from the earliest to the latest component capture, like an SDO cycle's sequential reads; the age guard applies
   to the earliest component, and a later report is a new cycle only when every component is a newer sample. Arrival
   order, a shared SYNC or event timer and a new host epoch never make samples one instant (Q27, D11), and capture
   after the acknowledgement is a host ordering bound, not a measured drive sampling time. A missing, pre-command or
   unqualified component, including old-map same-ID bytes under AGENTS.md's PDO route-retirement rule (D11), leaves
   that decision on SDO reads. Heartbeat consumer time is the node's liveness, separate from state freshness.
5. **Kept invariants.** Motion starts, enable, homing, save and NMT are never replayed. No optional guard or age cap
   suppresses an otherwise admissible stop, and a stop keeps priority and its reserved urgent capacity over ordinary
   work. Conditions that are not guards still refuse a stop by name: missing transport, an inhibited controller, a
   blocked route or NMT state, an invalid binding, exhausted request identities, a full urgent or retained-result
   slot, or a HALT predecessor from which `010F` could enable. Earlier TX on its channel that has not retired makes
   the stop wait; it is never cancelled or bypassed. The library never substitutes another stop kind (the best-effort
   `000B` branch stays the application's explicit policy), evicts a retained result or skips TX retirement to make a
   stop accepted. Uncertainty after an unconfirmed write is reported; a best-effort stop stays labelled unconfirmed.
   Defaults live in one documented place and are reported in health and results.

Boundaries: guards are application settings reachable through the library API; examples and the console only call
that API. No hidden retry, clock or I/O enters the portable core.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its timeout, deadline and freshness seams; RS is
authoritative for common names. Inspect FieldCore-node read-only for its per-device response timeout and command
deadline settings ([audit](../reports/2026-11-17_fieldcore_rs485_audit.md)). If HEAD, dirty state and seam match the
last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or
rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read
`Reads.h`, `Position.h`/`.cpp`, `Velocity.h`/`.cpp`, `Actions.h`/`.cpp`, `Homing.h`/`.cpp`, `PdoState.h`,
`CanBusOwner.cpp` and `SdoClient.cpp` (`cutoff`, `checkShape`, `expire`), `src/Axis.cpp`, `AxisSession`, `ProbeMotion`,
`ProbeConsoleMotion.cpp`, `MotionIntent.cpp`, `BenchRoute.h`, `BenchConsole.cpp`, the prompt 17, 18, 51, 52 and 53
reports, prompt 56's report and measured reply times, backlog D11, and Q11 and Q27 in the ledger.
Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Inventory every timing rule in the library, the examples, the bench, the console and the Python runners: value,
   owner, purpose, and whether it is a causal rule or a policy guard. Record it in the report; it drives the changes.
2. The per-node response timeout in the binding and `ReadOptions`/service options, with its default and range; remove
   the separate queue setting and derive every attempt's queue, TX-handoff and reply cutoffs from the timeout (design
   item 1), finite also with no operation deadline; derive service counts from it. Restate prompt 31's A-20 rule (a stop's
   maximum age at least its fields times the queue and response timeouts) on the derived cutoffs.
3. Optional deadlines: "no software deadline" for moves and homing in the library and the owner (an explicit state,
   optional `maxPolls`, named exhaustion outcomes), the console's estimate under design item 2's input rule with
   `deadline MS|none` and the deadline's source in replies, and the replacement window counted against the remaining
   deadline only.
4. `maximumAgeUs` as an optional guard with per-kind defaults and "off"; keep the causal ordering checks; every refusal
   names its guard. Stops follow design item 5: no guard suppresses an otherwise admissible stop, and each non-guard
   refusal keeps its own name.
5. TPDO-fed state: record each decision's minimum field set, then let the session and `ProbeMotion` take a report from
   qualified TPDO samples for admission, observation and stop naming under design item 4 (one TPDO where the set fits,
   otherwise per-field assembly), with a settable sample age; SDO fallback for any decision whose report is
   incomplete. This prompt adds no remapping.
6. Bench and console: route and arming budgets from the per-node timeout; the arming age becomes a settable guard (the
   paired arming of route protocol 16 stays available); a `timing` console command that reads and sets the guards
   through the API.

## Verify and exit

Host tests: a move with no software deadline runs to completion and still stops on request; with no deadline, a
stalled queue, a pending TX and a lost reply each end that service attempt at its derived cutoff with its effect
evidence kept while another node stays serviceable, and homing with no poll bound and step or request identity
exhaustion end with their named outcomes; the console's estimate for slow and long moves, and documented outcomes for
an unscaled native move, an absolute target without a current position, zero displacement, reverse travel and
overflow, with estimated, explicit and none deadlines distinguishable; the replacement window at short timeouts; each
guard on and off, with its refusal naming it; full urgent or result capacity, missing transport, an invalid binding
and an unsuitable HALT predecessor give their own outcomes with no substituted stop kind, and ordinary work cannot take
the reserved stop slot; causal ordering still refuses a report older than its command; a TPDO-fed stop after a stale
SDO cycle is sequenced; a missing, pre-command, unqualified or old-map same-ID component leaves that decision on SDO
reads; reordered or duplicate frames never form a new cycle or narrow its window; stale velocity beside fresh status
is refused by name with the age guard on and, with it off, reported with its true window, never as one instant; no
replay anywhere. Run the native, firmware, stack and documentation gates and CI on the actual final commit. Bench: a
30 s console move at low speed, a replacement late in a move, the two-axis trials without the paired arming, a stop
5 s after a HALT, then a short regression.

## Documentation and delegation

Update the axis contract, velocity, position and stop guides, the CLI contract, the owner guide, the shared deadline
rule ("Deadline cutoff" in [CONVENTIONS.md](CONVENTIONS.md)), the bench README, coverage, backlog (D11 where it
changes), CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the
causal-ordering rules and the TPDO-fed state; normal coding agents own disjoint tests. The lead integrates and runs
combined checks. Record `docs/reports/prompt_73.md`. Commit and sync on the current branch before handoff.
