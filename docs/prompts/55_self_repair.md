# Prompt 55 Communication self-repair, node presence and retry policy

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md), [42](42_cli_motion_routes.md), [45](45_fault_integration.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "Stuck read/write - this has to be solved, there has to be mechanism to
selfrepair - retransmissioning, etc - pick the industry simplest safe functional standard"; "One frame no drive answers
blocks all sending until a recovery - find very simple and safe procedure for handling this - like the drives act
independently and firmware assume their independence (need very simple industry standard solution for handling
this)"; "No automatic retry, replay or fault clear - add this functionality - you can get inspired in fieldcore-node
firmware in rs485 - check whether its viable and well applicable and add it into the promptset (or pick better
alternative - simple, functional is enough for now - standard in CAN systems). It seems to me it should be connected
with the 'one frame no drive answers' problem - check that together".

Revised scope (2026-11-17). The user asked for the policies to be implemented as a reference solution in the examples,
taken from FieldCore-node's RS485 handling, with "more responsibility on the end user of this firmware with possible
optional settable guards" (verbatim in [prompt 74](74_reference_self_repair_example.md)). This prompt therefore delivers
only the library primitives that prompt 74's example module uses; the presence state machine, retry policy,
reconciliation, the controller-restart schedule and optional fault clear are prompt 74's, its one policy
implementation. The [FieldCore-node audit](../reports/2026-11-17_fieldcore_rs485_audit.md) replaces the earlier
comparison notes.

Observations ([self-repair report](../reports/2026-11-12_self_repair_and_mixed_protocols.md)); refresh them against
current code before implementation:

- The bench's unresolved routes required MCU restarts (Q18; four on 2026-11-04, D14). A client abort has no server
  acknowledgement. An SDO reply carries no request identifier and a download acknowledgement does not echo the written
  value, so a late reply to the same object cannot be told apart from a current one; a reply to another object can.
- A healthy CAN receiver may ACK a frame regardless of its software filter, so no ACK is controller evidence, not proof
  that the target drive is absent. In the observed error-passive ACK-error case retransmission never reached bus-off
  (TEC 128, about 7250 ACK errors/s, one frame retried for 4.9 h on 2026-11-03). The hazard is late delivery of that
  stuck frame when a node returns.

Deliver, as library and adapter primitives with documented defaults (the portable core keeps no hidden retry, as
AGENTS.md requires):

1. **SDO timeout resync:** on a response timeout after a possible transmission the client sends one abort `0x05040000`
   (SDO protocol timed out). The abort is cleanup only and settles nothing. A quarantined route reopens only under a
   reuse policy, a settable guard whose documented default is *accepted risk with marker* (named in health and on every
   affected result; *none* keeps the route quarantined as today). *Accepted risk with marker* reopens after local TX
   retirement of the request and the abort and one successful read of a different object (the marker; the owner sends it itself as the resync's last step, one read of a configured accepted object,
   default `1000:00`, accepted-unqualified under DEC-1, through `admitDrainMarker`, counted and logged; under *none*
   nothing is sent); *accepted risk without marker* reopens after that local TX retirement alone, as CANopenNode and
   python-canopen do. Either setting states the existing `SdoDrainPremises` (sole client, ordered server) as accepted,
   not established; Q18 stays unqualified, and nothing is reported as a CiA guarantee or a measured bound. Neither
   setting can detect a delayed reply naming the same object as a later request: after `A(x)` times out and the route
   reopens, a new `A(x)` can complete on the old reply. Every later result on a route reopened this way therefore
   carries an accepted-correlation mark for the rest of that binding. The marker adds observable checks only: a
   retained reply arriving during it is absorbed, another object's frame fails it, and a marker mismatch invalidates the
   candidate binding; its success proves no server ordering or remote retirement. Observed contradicting traffic
   (retained-pair overflow, a frame with no multiplexer, an unexpected frame after a reopening) stops reuse on that
   route under either setting, is reported, and needs a new explicit application decision (an owner call that re-accepts reuse on that route;
   prompt 74 never makes it automatically), not an MCU restart. A report from a reopened route meets prompt 73's
   causal ordering as accepted, not established, and every decision built on it carries the mark. Local TX
   retirement is never remote retirement, and historical effects stay uncertain.
2. **TX lifetime and controller restart:** a frame without TX-done within its lifetime (settable, with a documented default; prompt 73
   derives it from the node's response timeout) is reported expired; it may still
   leave until a controller restart retires it as possibly sent, and nothing is resent. A restart entry point performs
   the existing delete/recreate recovery, single-flight and logged; the application decides when to call it (an
   explicit recovery, or prompt 74's schedule with its due time, backoff and rate limit). The interface enters
   NO_LISTENER until a received frame or one ACKed frame. ACK or RX restores only controller evidence, not identity,
   route freshness or motion readiness. Bus-off and RX overflow use the same entry point and keep their own causes.
3. **Presence inputs:** per node, the owner exposes the last response, last heartbeat, boot-up and consecutive
   no-response counts as observations; it decides nothing. Boot-up still invalidates bindings, motion evidence and
   sessions.
4. **Retry classification:** every prepared operation states its class: R (an upload whose `readEffect` is
   `REVIEWED_NON_CHANGING`, or `ACCEPTED_UNQUALIFIED` under DEC-1), W (idempotent absolute setting with readback), M
   (motion start, enable, homing, save, NMT) or N (not repeatable), the default for everything else, including
   `UNQUALIFIED` reads such as the fault and history reads `603F`, `1001` and `1003`. The class never changes
   `readEffect`: an application's decision to repeat an accepted read leaves its unknown effect unknown. An application may also promote one named object to R by an explicit,
   recorded permission; its `readEffect` stays as it was. The library
   never retries. A quick stop can be issued again as a new, separately identified operation; the HALT word
   `010F` is never repeated (from Switched On it could enable); M operations are never replayed, because an
   unconfirmed start may already have run (a relative +10 mm move sent twice becomes +20 mm).
5. **Fault clear** stays `prepareClearAlarm` with its current gates; the opt-in policy is prompt 74's. The post-FF05
   enable fault (backlog S21, prompt 72) is preserved.

Boundaries: every primitive is reported in health and results; no deadline is extended by a resync; stops keep
priority over every repair step; uncertainty after an unconfirmed write is never erased by a repair; boot-up, bus-off
and restart still invalidate dependent evidence.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its retry classification
(`docs/architecture.md` "Only explicitly classified operations may be retried under application policy") and recovery
seams; RS is authoritative for common names. Inspect FieldCore-node read-only for its RS485 timeout, retry, offline and
re-init policy, starting from the audit. If HEAD, dirty state and seam match the last recorded row, write "unchanged
since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This
does not relax vendor evidence, safety, uncertainty or verification rules. Read the reports, `SdoClient`,
`CanBusOwner` recovery, the TWAI adapter and SDK notes, the prompt 14 and 24 reports and backlog D14, H04, Q18. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Decision records: the ACK and error-passive facts, the resync rule as an amendment to DEC-3 with its named reuse
   policy and the residual risk of each setting, the TX lifetime rule, the retry classes.
2. The timeout abort and resync through `CanBusOwner::admitDrainMarker` and owner recovery; `SdoClient::checkDrain` and
   `admitDrain` stay internal to that path; no second SDO engine. The named policy is how the application states the
   premises, recorded as accepted; the setting without a marker reuses `reopenAfterDrain`'s effects (reservations
   invalidated, experiment grants void, `requalifications` counted), not a second reopening.
3. TX lifetime expiry, the single-flight, logged restart entry point and the NO_LISTENER state; no restart schedule.
4. Presence observations per node in the owner's health.
5. The retry class on every prepared operation and the quick-stop reissue as a new operation.
6. Console and health output for each primitive; console settings call the API only.
7. Bench, with the bench image naming its reuse policy in the evidence: a short response-timeout read on node 1 with
   repeated recoveries (for example 1000), each completed or retained unresolved before the next; the 2026-11-04 read
   cases without a restart; an absent node ID beside nodes 1 and 2; one physical action by the user: unplug the S2's
   CAN connector for 10 s with the drives released (expiry reported within the lifetime, one restart called by the
   bench image, no stale frame, both nodes resynced); a slow move with a forced trigger timeout and no replay; a
   quick-stop reissue.

## Verify and exit

Host tests: exact abort bytes and no abort when TX was never possible; a late reply to the marker's other object
rejected; in `test/sdo_reconciliation_test.cpp`, `A(x)` times out, the route reopens (marker `B` succeeds, or no
marker), a new `A(x)` starts and the old `A(x)` reply arrives: under both accepted settings the new request completes
on it and its result carries the accepted-correlation mark, with no detection claimed, and with no reuse policy the
route stays quarantined and refuses the marker; an unexpected frame after a reopening stops reuse under both settings;
marker mismatch invalidates the binding; other channels unaffected; TX lifetime expiry, no resubmit, TX-done racing
expiry or a stop, no restart without a call and one per call; bus-off and RX overflow on the same path; presence
observations of two nodes with one silent; N as the default class, R only for the two listed read effects with
`readEffect` unchanged; one trigger on the wire after an uncertain move; the quick-stop reissue sends only `000B`;
boot-up invalidation. Run the native, firmware, stack and documentation gates and CI on the actual final commit.

## Documentation and delegation

Update the owner, CANopen services and TWAI SDK guides, the README, coverage, backlog (D14, H04, T04) and the
limitations report items 2.5, 7.3 and 8.3, CHANGELOG and Doxygen in the same stage, keeping host-tested and measured
separate. The services guide's "Settling and reopening a route" records the named reuse policy as a contract change:
its "Client abort queued, handed off or CAN-ACKed" row changes for the setting without a marker, the drain-marker
rows for the setting with it, and its statement that the premise is a gate gives way to the documented default; the
negative test stays for policy *none*. PROGRESS rows 13 and 14 and the ledger's Q18 text change where they say Q18
blocks real routes. A stronger reviewer challenges the resync rule, the restart rules and the retry
classes; normal coding agents own disjoint tests. The lead integrates and runs combined checks. Record
`docs/reports/prompt_55.md`. Commit and sync on the current branch before handoff.
