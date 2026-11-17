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
reconciliation and optional fault clear are prompt 74's. The [FieldCore-node audit](../reports/2026-11-17_fieldcore_rs485_audit.md)
replaces the earlier comparison notes.

Observations ([self-repair report](../reports/2026-11-12_self_repair_and_mixed_protocols.md)); refresh them against
current code before implementation:

- The bench's unresolved routes required MCU restarts (Q18; four on 2026-11-04, D14). A client abort has no server
  acknowledgement, and a late reply to the same object cannot be told apart from a current one; a reply to another
  object can.
- A healthy CAN receiver may ACK a frame regardless of its software filter, so no ACK is controller evidence, not proof
  that the target drive is absent. In the observed error-passive ACK-error case retransmission never reached bus-off
  (TEC 128, about 7250 ACK errors/s, one frame retried for 4.9 h on 2026-11-03). The hazard is late delivery of that
  stuck frame when a node returns.

Deliver, as library and adapter primitives with documented defaults (the portable core keeps no hidden retry, as
AGENTS.md requires):

1. **SDO timeout resync:** on a response timeout the client sends one abort `0x05040000` (SDO protocol timed out),
   retires its local TX, then reopens the channel after one read of a different object (the marker). The marker is a
   settable guard, on by default; with it off, the application accepts the residual risk of a delayed same-object
   duplicate, which the result reports. Q18 stays unqualified either way; nothing is reported as a CiA guarantee or a
   measured bound. A marker mismatch invalidates the candidate binding, not historical effects.
2. **TX lifetime and controller restart:** a frame without TX-done within its lifetime starts the existing
   delete/recreate recovery, with backoff and a rate limit; nothing is resent; the interface enters NO_LISTENER until
   one bounded, configured probe or received frame. ACK or RX restores only controller evidence, not identity, route
   freshness or motion readiness. Bus-off and RX overflow use the same machinery and keep their own causes.
3. **Presence inputs:** per node, the owner exposes the last response, last heartbeat, boot-up and consecutive
   no-response counts as observations; it decides nothing. Boot-up still invalidates bindings, motion evidence and
   sessions.
4. **Retry classification:** every prepared operation states its class: R (non-consuming read), W (idempotent absolute
   setting with readback) or M (motion start, enable, homing, save, NMT). The library never retries. A quick stop can
   be issued again as a new, separately identified operation; the HALT word `010F` is never repeated (from Switched On
   it could enable); M operations are never replayed, because an unconfirmed start may already have run (a relative
   +10 mm move sent twice becomes +20 mm).
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

1. Decision records: the ACK and error-passive facts, the resync rule with its marker guard, the TX lifetime rule, the
   retry classes.
2. The timeout abort and resync through `CanBusOwner::admitDrainMarker` and owner recovery; `SdoClient::checkDrain` and
   `admitDrain` stay internal to that path; no second SDO engine.
3. TX lifetime with automatic, bounded, logged controller restart and the NO_LISTENER state.
4. Presence observations per node in the owner's health.
5. The retry class on every prepared operation and the quick-stop reissue as a new operation.
6. Console and health output for each primitive; console settings call the API only.
7. Bench: a short response-timeout read on node 1 with repeated recoveries (for example 1000), each completed or
   retained unresolved before the next; the 2026-11-04 read cases without a restart; an absent node ID beside nodes 1
   and 2; one physical action by the user: unplug the S2's CAN connector for 10 s with the drives released (a restart
   within the lifetime, no stale frame, both nodes resynced); a slow move with a forced trigger timeout and no replay;
   a quick-stop reissue.

## Verify and exit

Host tests: exact abort bytes and no abort when TX was never possible; a late reply to the marker's other object
rejected; a same-object late reply with the guard on and off, reported; marker mismatch invalidates the binding; other
channels unaffected; TX lifetime expiry, no resubmit, TX-done racing expiry or a stop, backoff and rate; bus-off and
RX overflow on the same path; presence observations of two nodes with one silent; one trigger on the wire after an
uncertain move; the quick-stop reissue sends only `000B`; boot-up invalidation. Run the native, firmware, stack and
documentation gates and CI on the actual final commit.

## Documentation and delegation

Update the owner, CANopen services and TWAI SDK guides, the README, coverage, backlog (D14, H04, T04) and the
limitations report items 2.5, 7.3 and 8.3, CHANGELOG and Doxygen in the same stage, keeping host-tested and measured
separate. A stronger reviewer challenges the resync rule, the restart rules and the retry classes; normal coding agents
own disjoint tests. The lead integrates and runs combined checks. Record `docs/reports/prompt_55.md`. Commit and sync on
the current branch before handoff.
