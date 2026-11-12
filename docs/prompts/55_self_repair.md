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

Facts ([self-repair report](../reports/2026-11-12_self_repair_and_mixed_protocols.md)):

- A stuck SDO channel needs an MCU restart because the only way back, the drain marker, needs premises (`soleClient`,
  `orderedServer`) that no fixture sets (Q18; four restarts on 2026-11-04, D14). The library also never sends the
  standard SDO abort `0x05040000` on timeout. CANopenNode and python-canopen send the abort, go idle and reuse the
  channel at once, filtering late replies by command, index/subindex and toggle.
- Every receiving node ACKs every correct frame, so "no ACK" means no other node is listening at all; one missing
  drive never stalls the bus, its SDO only times out. An error-passive transmitter's ACK errors do not raise TEC, so
  no-ACK retransmission never reaches bus-off (measured: TEC 128, about 7250 ACK errors/s, one frame retried for 4.9 h
  on 2026-11-03). The real hazard is late delivery of that stuck frame when a node returns.
- FieldCore-node's RS485 policy maps (per-device timeout, bounded read retries within the deadline, miss counter,
  online on next success, urgent stop first, transport re-init without replay); its half-duplex master/slave mechanics
  do not.

Deliver, as explicit application-configured policies in the owner and adapters (the portable core keeps no hidden
retries, as AGENTS.md requires: applications own retries and health policy):

1. **SDO resync:** on timeout one abort `0x05040000`, then a guard time once local TX is retired, then the existing
   different-object drain-marker read; reopen on a matching value, presence miss on timeout, binding invalidation on a
   wrong value. This adopts the CiA 301 "one request, one reply, in order" premise as decision DEC-3a, the industry
   standard the user chose; Q18 is then measured to confirm it.
2. **TX lifetime and automatic controller restart:** a frame without TX-done within its lifetime starts the existing
   delete/recreate recovery automatically, with backoff and a rate limit; nothing is resent; the interface enters
   NO_LISTENER until a frame is received; the same path serves bus-off and RX ring overflow.
3. **Per-node presence:** DEGRADED after M misses, OFFLINE after N (FieldCore uses 2 and 5), fail-fast for an OFFLINE
   node, one slow probe, online again on success or boot-up; never a node reset on loss. Each axis stays independent.
4. **Retry classes:** reads retry after resync; setting writes only through readback; the quick stop `000B` may be
   repeated; the HALT word `010F` is never repeated (from Switched On it could enable); motion starts, enable, homing,
   save and NMT are never replayed, because an unconfirmed start may already have run (a relative +10 mm move sent
   twice becomes +20 mm); after an uncertain motion command, motion is inhibited until a fresh stopped read.
5. **Optional fault clear:** off by default, only the existing `prepareClearAlarm` for resettable alarms (FF05), only
   when released and idle, limited per window and per boot, never enables.

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
2. The timeout abort frame, then the SDO resync policy.
3. TX lifetime with automatic, bounded, logged controller restart and the NO_LISTENER state.
4. Per-node presence with the thresholds and probe.
5. The retry policy with the class table, and the optional fault clear.
6. Console and health output for each policy; console settings call the API only.
7. Bench: a 1 ms response-timeout read on node 1 that must self-repair, 1000 repeats; the 2026-11-04 cases replayed
   without a restart; the Q18 measurement (slow write then read, a second request 0.5 ms after the first, duplicates);
   an absent node ID beside nodes 1 and 2 going OFFLINE while they keep their rate; one physical action by the user:
   unplug the S2's CAN connector for 10 s with the drives released (expect a restart within the lifetime, no stale
   frame, both nodes resynced); a setting write with a short timeout; a slow move with a forced trigger timeout and no
   replay; a stop repeat.

## Verify and exit

Host tests from the report: exact abort bytes and no abort when TX was never possible; late frame inside the guard;
same-object late reply ignored during the marker; wrong marker value; reopen invalidates reservations; other channels
unaffected; TX lifetime expiry, no resubmit, TX-done racing expiry or a stop, backoff and rate; bus-off and RX overflow
on the same path; two nodes with one silent; one trigger on the wire after an uncertain move; readback-confirmed
setting retry; stop repeat sends only `000B`; fault clear limits and no enable frame. Run the native, firmware, stack and
documentation gates and CI on the actual final commit.

## Documentation and delegation

Update the owner, CANopen services and TWAI SDK guides, the README, coverage, backlog (D14, H04, T04) and the
limitations report items 2.5, 7.3 and 8.3, CHANGELOG and Doxygen in the same stage, keeping host-tested and measured
separate. A stronger reviewer challenges DEC-3a, the restart rules and the retry classes; normal coding agents own
disjoint tests. The lead integrates and runs combined checks. Record `docs/reports/prompt_55.md`. Commit and sync on the
current branch before handoff.
