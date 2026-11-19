# 09 — Asynchronous reports, unacknowledged dispatch and reply matching

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 02b's matcher and the completion phases of 04 and 05. MK-D09, D11, D12, D17 and D36 are
central. The protocol has no sequence numbers or transaction IDs: a reply is matched only by identifier and echoed
code (review section 14 rule 2).

## Work

1. Write the matcher contract into the profile header and `docs/mks_servo_can_coverage.md`, extending 02b:
   - per interface and slave address: one outstanding request, one pending completion phase bound to an admitted
     motion generation, 02b's ambiguous-shape set and a stream table per code;
   - a frame matches a request only by identifier, echoed code, accepted length, check byte and expected phase;
   - a frame matching nothing is a raw unsolicited observation with receive time, copied through the owner's observe
     path, counted in profile evidence, never attributed to a later request;
   - an unsolicited 2 or 3 with no pending phase (cancelled, superseded, timed-out or pre-restart motion) is never
     attributed and its shape enters the set; with a pending phase it ends that phase under 04's and 05's rules only
     when 02b step 6 attributes it (`QUALIFIED`, or `QUIET_INTERVAL` reported as that risk).
2. Late and ambiguous replies follow 02b step 6 and nothing weaker: entry at the end of every exchange, also a
   completed one, and on cancellation, timeout, possibly sent without reply, duplicate, same-code stop, supersession,
   rebinding, restart and first attachment; `possiblyLate` publication also while a later same-shape exchange is
   pending; retirement only by a recorded drive power cycle; a quiet interval, judged by the owner capture time,
   gives `QUIET_INTERVAL` correlation, never reported or consumed as `QUALIFIED`. A host controller restart or
   a new host generation retires nothing, because the drive may still send. 12A Q9a measures reply latency to inform
   the application's choice; the library invents no default. This stage adds the stream (step 3) and multi-target
   (step 6) entries and writes the whole rule into the header, coverage file and user guide.
3. Periodic reports 01H (V1.0.9 tables only): `[01, code, period ms]`, period 0 stops; setup reply `[01, code, status]`;
   then unsolicited `[code, value]` frames identical in form to the polled reply (MK109 p.36). Implement explicit
   `prepareReport` and `prepareReportStop` as settings operations (persistence unknown, treated as possibly retained):
   - only codes the ledger marks read-only and reportable; the manual's example is 31H, any other code stays unknown
     until 12A Q12 or a document confirms it;
   - stream state per address and code is `OFF`, `ACTIVE` or `UNKNOWN` on the record 03 step 8 lands (initial
     `UNKNOWN`, the caller's `OFF` declaration, a sample while `OFF`); this stage adds the 01H transitions. Every code
     the declared dialect could report (any read-only code while the reportable set is unconfirmed; none on `V1_0_0`,
     which has no 01H) is `UNKNOWN` at
     first bind, rebind, a declaration change, controller restart or bus-off and host restart, because a used drive
     may run a stream another tool set and 01H persistence is unknown. A setup or stop with possible TX and no checked
     reply leaves `UNKNOWN`; an acknowledged setup gives `ACTIVE`; a period-0 stop whose checked success reply is
     attributed `QUALIFIED` gives `OFF` with that code's shape entered in 02b's set, so a sample queued before the
     stop's reply is never taken for a poll's reply. A stop reply attributed only `QUIET_INTERVAL` leaves `UNKNOWN`
     (its result reports that risk-accepted acknowledgement): the setup's reply has the same form (MK109 p.36), so a
     delayed setup success can answer a stop that never applied, and the interval accepts timing risk in
     attribution, not that a continuing stream stopped. 03 step 8's caller declaration with its evidence reference
     gives `OFF` (application policy, recorded); an
     unsolicited `[c, value]` frame while `OFF` makes it `UNKNOWN`. The dialect declaration never sets a state;
   - while the state is `ACTIVE` or `UNKNOWN`, polled reads of c on that address are refused before TX (03); stream
     samples carry `unsolicited` provenance, host receive time and no sequence;
   - gaps are detectable only by timing against the declared period; a missing sample is not a fault by itself.
4. Bus-level contention (inference from CAN arbitration): drive frames and host requests to that drive share one
   identifier with different data, so a simultaneous start is not resolved by arbitration but ends in a bit error in
   the data field, after which the controllers retransmit. Record controller error counters with streams active in 12A Q12; never treat retransmission as replay.
5. Reply policy 8CH (declared per binding, changed only in 10): respond 1, active 1 gives immediate and completion
   replies; respond 1, active 0 gives only the immediate reply, so completion needs F1H polling (MK109 p.47); respond 0
   gives no reply for at least motion commands. Its scope beyond motion is unstated (MK-D11; the menu says "in
   speed/position mode", p.26): with respond 0 declared, treat every command as `AcknowledgementSupport::NONE` and reads
   as unavailable until 12A Q6 resolves it. V1_0_0's single byte has no active option (MK100 p.19).
6. Unacknowledged dispatch, explicit native multi-target operations only:
   - broadcast ID 0 and group IDs (8DH) execute on every matching drive without reply (MK109 p.30-31, p.49, p.105):
     every powered drive on the bus for ID 0, every drive configured with the group ID otherwise, listed or not. An
     unlisted drive, an old-dialect drive or a latch whose host record was lost is reached all the same, and the library
     cannot detect an unknown drive (10 step 9). Dispatch therefore requires the application's commissioned, exhaustive
     participant record for the interface (every drive physically on it with slave ID, group ID, dialect, check mode and
     the evidence reference), checked by this stage's participant validator (10 step 9 reuses it); one declared dialect
     and check mode shared by every possible recipient (mixed dialects or check modes refuse outright until a ledger
     review establishes the same form and effect across them); the exact frame (code, form, payload, DLC) allowed for
     multi-target use by that dialect's reviewed `multi_target` row (01 step 3), so a code both V1.0.9 tables have does
     not admit a form only one documents (F7H 01, MK-D01); disjoint IDs; and authorization for the effect on each
     recipient. Anything missing, incomplete or stale, or 04 step 4's admission barrier on any possible recipient,
     refuses before TX. 4BH is never sent through this path: only step 7's latch-aware operation sends it. After
     dispatch each recipient's execution is `UNKNOWN`, a motion frame leaves it a retained obligation (04 step 4),
     its pending phases and references are invalidated and the code's shapes enter its ambiguous set, so a later 2
     on its own ID never
     qualifies a completion; never used by discovery, probe, recovery or a common single-axis verb;
   - V1.0.0 and V1.0.1 still replied to broadcast and group frames (implied by the V1.0.2 note, MK109 p.1), on an
     undocumented identifier: refuse broadcast and group dispatch on `V1_0_0`.
7. Synchronization flag (V1.0.9 tables; since 1.0.8): 4AH enables latching (a setting through 08's context); a latched
   motion command answers 5 and waits for 4BH, sent once to ID 0 without reply (MK109 p.106-107). The library sends 4BH
   once per explicit request; the manual's repeat about every 1 ms is an application choice, and whether a second 4BH
   restarts or repeats a latched move is undocumented (MK-D12), so the operation records each 4BH as new intent. 4BH
   goes to ID 0, so it needs step 6's exhaustive participant record and a latch state for every possible recipient,
   kept from its 4AH setting and every motion command this host sent it since a recorded power cycle (02b step 6)
   while 4AH was or may have been enabled, with each command's reply (5, other or missing). No page says that a local
   cancel, a stop or 4AH 00 discards a held command (MK109 p.106-107), so only a recorded power cycle clears a latch
   state:
   - `NONE`: 4AH established off, or no motion command sent while it may have been on;
   - `LATCHED_LIVE`: a motion with a checked status 5 whose admitted operation is still pending (not cancelled,
     superseded, stopped or failed) and before its deadline;
   - `LATCHED_STALE`: such a latch after its operation ended locally without a 4BH; the drive may still hold the
     command, and the next 4BH would start an expired or cancelled motion;
   - `POSSIBLY_ARMED`: a latched member after a 4BH that may have been sent, kept beside any later latch, because
     whether the latch clears or a further 4BH repeats the move is undocumented (MK-D12);
   - `UNKNOWN`: first bind, rebind, restart, a lost record or a latched request with a missing reply.
   Two separate gates apply to every 4BH. The TX-order barrier is 04 step 4's motion record: every possible
   recipient's record holds nothing or only a live latched staging with its attributed status 5 (no open TX, uncertain
   execution or retained obligation, no other live intent), each live deadline still ahead at the 4BH handoff (the
   4BH deadline is at most the earliest); nothing overrides it. The latch-state gate: by default 4BH is admitted only
   when every possible recipient is `NONE` or `LATCHED_LIVE`; `POSSIBLY_ARMED`, `LATCHED_STALE` or `UNKNOWN` on any
   recipient refuses before TX. The caller's explicit, recorded per-drive acceptance admits 4BH over such a
   recipient without a recorded power cycle (lead decision, round 3), answering this gate only; that drive's result
   then states that any latched move on it, an expired, cancelled or earlier one included, may execute, and its
   motion record gains a retained obligation (04 step 4).
   After a possible 4BH TX each latched member's execution is `UNKNOWN` and its latch `POSSIBLY_ARMED`. A latched
   move has no status 1 (its staging reply is 5 and 4BH has none), so its outcomes stay separate and none imports 05
   step 7's status-1 predicate: `stagingAccepted` (the attributed status 5), `triggerPossiblySent` (the 4BH's TX
   disposition sent or possibly sent, never "executed") and, per native form:
   - F4H and F5H (encoder targets): `latchedCompleted`, its terminal evidence under 04 step 4, needs a pre-trigger
     31H read handed off after the status 5 and before the 4BH handoff whose value lies outside the caller's
     tolerance of the latched target (for F4H, that value plus the displacement), then 05 step 7's standstill window
     with every read handed off after the 4BH's final TX report, all attributed with 31H's stream state `OFF`. The
     pre-trigger read is what shows that the axis reached the target after the trigger. A target already within
     tolerance, a 4BH that did not arrive or a window ending elsewhere leaves execution `UNKNOWN` and the obligation
     for 04's stop; it is never claimed on a drive admitted under the per-drive acceptance, where another held move
     may have run;
   - FDH and FEH (pulse forms; 33H is no endpoint proof until MK-D30 is resolved) and F6H (no position target): no
     reviewed completion route; execution stays `UNKNOWN` and only 04's stop with its post-stop observation
     discharges the obligation.
   A later status 2 on the member's ID, attributed by 02b step 6 (`QUIET_INTERVAL` at best, the latched exchange
   having entered the shape), is a native observation that completes nothing and ends no phase alone. Members
   without a latched move are `UNKNOWN` until their own observation. Native names only; never the CANopen `sync`
   verb, prompt 51's SYNC producer or any SYNC counter.
8. Silent mode (50H 02) is never sent; a drive left silent by another tool times out on everything and looks absent
   (MK-D32). Malformed frames are never sent deliberately outside an authorized 12A question (MK-D09).
9. Evidence classes for health (prompt 55): reply timeout, unmatched frame, possibly-late reply, checked rejection
   (status 0) and stream gap, per address. No CANopen heartbeat, EMCY or NMT state is synthesized. Retry classes come
   from the ledger; only reads under 01's decision may be repeated, as new operations within the deadline.

## Deliver

Extended matcher and stream table, `Reports.h/.cpp` with `ReportContext`, `Group.h/.cpp` (or the profile's multi-target
module) for broadcast, group and 4AH/4BH operations and the participant validator, `test/mks_servo_can_async_test.cpp`,
owner-backed app and console routes (`mks report`, `mks group`, `mks latch`, `mks execute-latched`), API/CLI parity
(console routes and parity once S4 lands), coverage and user-guide sections on matching limits, report and PROGRESS row.

## Acceptance

Through the real owner and fake drive: stream frames interleaved with polled replies of other codes; polled read of a
streamed code refused; a stream active before the host starts (state `UNKNOWN`, poll refused, sample observed); a
lost setup acknowledgement leaving `UNKNOWN`; a sample queued after an acknowledged report stop not taken as a poll's
reply; an applied setup, a stop the fake drive does not apply and a delayed setup success captured after the quiet
interval answering the stop: state `UNKNOWN`, the stop's result `QUIET_INTERVAL`, a later polled read refused before
TX and zero dependent changing TX; unsolicited 2/3 with and without a pending phase; an old status 2 arriving during a new same-code move, before
and after the chosen quiet interval, never `QUALIFIED`, also when the old move had completed and its first delayed
duplicate arrives; a duplicate terminal frame; late replies `possiblyLate` until a
recorded power cycle; respond-off and active-off policies; broadcast and group operations invalidating every member;
an incomplete participant record, a mixed-dialect or mixed-check-mode recipient (F7H 01 to a group with one MK109A
member), a form the dialect has but its `multi_target` row does not allow (8BH to a group), 4BH through the generic
dispatch and a lost latch record refusing before TX; 4BH sent once
with no reply and no repeat; a latched member whose operation was cancelled or expired (`LATCHED_STALE`), an
`UNKNOWN` latch and a further 4BH after a possibly sent one each refusing before TX without the per-drive acceptance
and admitted with it, the result stating that any latched move on that drive may execute and its record gaining a
retained obligation; 4BH refused before TX for a `LATCHED_LIVE` member whose staging frame is still open TX, and the
per-drive acceptance never admitting 4BH over open TX, uncertain execution, a retained obligation or a non-latched
live intent; a live F5H latch whose 4BH the fake drive drops (execution `UNKNOWN`, obligation kept for 04's stop), the
same latch with a pre-trigger 31H outside tolerance and a real post-trigger standstill window at the target
(`latchedCompleted`, then a new motion admitted), a target already within tolerance (no `latchedCompleted`), and FDH,
FEH and F6H latches with no completion route; an attributed status 2 alone completing nothing and no status 1
invented; V1_0_0 broadcast refused; a restart leaving
previously active streams unknown until stopped or declared off. The full matrix of contract 02 "Registration
acceptance" for this major change and CI on the exact final pushed commit pass. Hardware NOT RUN. Commit and push.
