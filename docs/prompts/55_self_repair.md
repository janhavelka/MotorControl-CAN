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

Historical observations ([self-repair report](../reports/2026-11-12_self_repair_and_mixed_protocols.md)); refresh these against current code before implementation:

- The bench's unresolved routes required MCU restarts (Q18; four on 2026-11-04, D14). A restart of this host alone
  is not proof that remote queued responses were retired. The drain marker requires `soleClient`, `orderedServer`
  and the service contract's absence of delayed duplicates from completed initiation/segment exchanges. The SDK,
  remote server and any bridge must justify those premises; ordering or a stable marker value alone is insufficient.
  Inspect the current timeout/abort path in `SdoClient` and the pinned CANopenNode source before adding cleanup.
  A client abort has no server acknowledgement and does not establish remote retirement. Other stacks' immediate
  channel reuse is a policy comparison, not proof that indistinguishable late replies can be correlated here.
- A healthy participating CAN receiver may ACK a correct frame regardless of its software acceptance filter;
  silent, disconnected, wrong-bitrate or faulted receivers cannot be assumed to ACK. No ACK is controller evidence,
  not proof that the target drive is absent. A missing drive does not itself stall TX if another receiver supplies
  ACK; its SDO still times out. In the observed error-passive ACK-error case,
  no-ACK retransmission never reaches bus-off (measured: TEC 128, about 7250 ACK errors/s, one frame retried for 4.9 h
  on 2026-11-03). The real hazard is late delivery of that stuck frame when a node returns.
- FieldCore-node supplies per-device timeout/presence and transport-policy comparisons. Its current motor module
  rejects nonzero retries in `Rs485MotorModule::applyRuntimeConfig`; sensor retry policy is not an implemented motor
  precedent. Select CAN retry classes explicitly; do not transfer half-duplex master/slave mechanics.

Deliver, as explicit application-configured policies in the owner and adapters (the portable core keeps no hidden
retries, as AGENTS.md requires: applications own retries and health policy):

1. **SDO resync:** evaluate one bounded cleanup abort `0x05040000`, local TX retirement and a different-object marker
   through the existing service. Define DEC-3a's device/controller premises before permitting reuse, including
   delayed duplicates from earlier completed transfers and old queued TX/RX. A guard needs an evidenced bound;
   elapsed quiet time, a queued abort, FIFO ordering and a matching marker value alone do not retire remote work.
   Where a premise is an explicit application risk acceptance, name it as such and retain Q18 as unqualified;
   never report the policy as a CiA guarantee or a measured bound. Without the reviewed premises keep admission
   blocked and report the missing boundary. A mismatch invalidates the candidate binding, not historical effects.
2. **TX lifetime and automatic controller restart:** a frame without TX-done within its lifetime starts the existing
   delete/recreate recovery automatically, with backoff and a rate limit; nothing is resent; the interface enters
   NO_LISTENER. Allow one bounded, explicitly configured probe opportunity so a returning drive with heartbeat
   disabled can be detected; do not wait forever for unsolicited traffic. ACK or RX restores only its controller
   evidence, not identity, route freshness or motion readiness. Keep urgent-stop admission explicit. The same
   recovery machinery serves bus-off and RX overflow, while retaining their different failure causes.
3. **Per-node presence:** DEGRADED after M misses, OFFLINE after N (FieldCore uses 2 and 5), fail-fast for an OFFLINE
   node, one slow probe, presence online again on success or boot-up; never a node reset on loss. Boot-up still
   invalidates bindings, motion evidence and sessions; presence alone cannot reopen SDO or resume work.
   Each axis keeps independent intent/results, subject to one physical transmitter's capacity.
4. **Retry classes:** only reviewed non-consuming or explicitly DEC-1-accepted polling reads retry after resync;
   fault/history and other Q20-gated reads need their own reviewed repeat permission. Settings use reviewed readback
   and idempotence before any separate retry; readback alone does not retire old queued writes. The quick stop `000B` may be
   repeated; the HALT word `010F` is never repeated (from Switched On it could enable); motion starts, enable, homing,
   save and NMT are never replayed, because an unconfirmed start may already have run (a relative +10 mm move sent
   twice becomes +20 mm). Each policy-generated stop is a separately identified finite attempt, not a replay that
   rewrites the earlier result. Motion stays inhibited until the existing checked stop/reference prerequisites are
   satisfied; a single fresh status value cannot clear uncertainty or requalify the route.
5. **Optional fault clear:** off by default, only through `prepareClearAlarm` with its current identity/effect gates,
   released/idle prerequisites and per-window/per-boot limits. The FF05-to-resettable link is an inference (Q13),
   not a universal resettable whitelist. Preserve the post-FF05 enable fault recorded in backlog S21 and prompt 72;
   clearing an alarm never authorizes enable, retry of motion or automatic escalation to reset/persistence.

Boundaries: every policy is off unless the application configures it and is reported in health and results; no
deadline is extended by a retry; stops keep priority over every repair step; uncertainty after an unconfirmed write is
never erased by a repair; boot-up, bus-off and restart still invalidate dependent evidence.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its retry classification
(`docs/architecture.md` "Only explicitly classified operations may be retried under application policy") and recovery
seams; RS is authoritative for common names. Inspect FieldCore-node read-only for its RS485 timeout, retry, offline and
re-init policy. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not
reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor
evidence, safety, uncertainty or verification rules. Read the report, `SdoClient`, `CanBusOwner` recovery, the TWAI
adapter and SDK notes, the prompt 14 and 24 reports and backlog D14, H04, Q18. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

1. Decision and evidence records: the ACK and error-passive facts, DEC-3a (SDO premise), DEC-4a (TX lifetime), DEC-9
   (retry classes, fault clear off by default).
2. The bounded timeout-abort and SDO resync policy through `CanBusOwner::admitDrainMarker` and owner recovery;
   `SdoClient::checkDrain`/`admitDrain` stay internal to that path. First write a table of each reopening premise,
   source or scoped acceptance, negative test and retained result. Current `reopenSettledRoutes` checks protocol
   success and TX retirement only. Add a small policy hold when this workflow also requires a commissioned marker
   value: compare the retained typed value before ordinary work can reuse the route; mismatch stays blocked.
   A matching value satisfies only that predicate, not duplicate retirement. Distinguish one application submission
   from hardware CAN retransmissions under DEC-4. Do not add another SDO engine or drop existing preconditions.
3. TX lifetime with automatic, bounded, logged controller restart and the NO_LISTENER state.
4. Per-node presence with the thresholds and probe.
5. The retry policy with the class table, and the optional fault clear.
6. Console and health output for each policy; console settings call the API only.
7. Bench: after the reopening premises are reviewed, a 1 ms response-timeout read on node 1, with 1000 planned recoveries;
   first complete one recovery or retain its unresolved result before proceeding. A failed campaign stops and leaves
   that acceptance item incomplete; it does not turn one success into a 1000-run pass. A configured timeout is host policy,
   not a drive timing guarantee. Re-run the 2026-11-04 read cases
   without a restart; the Q18 measurement (slow write then read, a second request 0.5 ms after the first, duplicates);
   an absent node ID beside nodes 1 and 2 going OFFLINE while they keep their rate; one physical action by the user:
   unplug the S2's CAN connector for 10 s with the drives released (expect a restart within the lifetime, no stale
   frame, both nodes resynced); a setting write with a short timeout; a slow move with a forced trigger timeout and no
   replay; a stop repeat.

## Verify and exit

Host tests from the report: exact abort bytes and no abort when TX was never possible; late frame inside the guard;
same-object late reply ignored during the marker; duplicate from an already completed transfer, including an older
segmented exchange; a missing reviewed server/bridge premise (evidence or explicitly scoped application risk
acceptance) keeps reuse blocked; wrong marker value blocks ordinary admission before reopening; reopen invalidates reservations; other channels
unaffected; TX lifetime expiry, no resubmit, TX-done racing expiry or a stop, backoff and rate; bus-off and RX overflow
on the same path; two nodes with one silent; one trigger on the wire after an uncertain move; readback-confirmed
setting retry; stop repeat sends only `000B`; no-heartbeat return from NO_LISTENER through one bounded probe;
boot-up changes presence without clearing quarantine; fault clear limits and no enable frame. Run the native, firmware, stack and
documentation gates and CI on the actual final commit.

## Documentation and delegation

Update the owner, CANopen services and TWAI SDK guides, the README, coverage, backlog (D14, H04, T04) and the
limitations report items 2.5, 7.3 and 8.3, CHANGELOG and Doxygen in the same stage, keeping host-tested and measured
separate. A stronger reviewer challenges DEC-3a, the restart rules and the retry classes; normal coding agents own
disjoint tests. The lead integrates and runs combined checks. Record `docs/reports/prompt_55.md`. Commit and sync on the
current branch before handoff.
