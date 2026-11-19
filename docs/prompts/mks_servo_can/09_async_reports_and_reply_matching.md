# 09 — Asynchronous reports, unacknowledged dispatch and reply matching

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 02b's matcher and the completion phases of 04 and 05. MK-D09, D11, D12, D17 and D36 are
central. The protocol has no sequence numbers or transaction IDs: a reply is matched only by identifier and echoed
code (review section 14 rule 2).

## Work

1. Write the matcher contract into the profile header and `docs/mks_servo_can_coverage.md`, extending 02b:
   - per interface and slave address: one outstanding request, one pending completion phase bound to an admitted
     motion generation, and a stream table per code;
   - a frame matches a request only by identifier, echoed code, accepted length, check byte and expected phase;
   - a frame matching nothing is a raw unsolicited observation with receive time, copied through the owner's observe
     path, counted in profile evidence, never attributed to a later request;
   - an unsolicited 2 or 3 with no pending phase (cancelled, superseded, timed-out or pre-restart motion) is never
     attributed; with a pending phase it ends that phase under 04's and 05's rules.
2. Late replies: after a timeout the address is `possiblyLate` for that code. No device timing is documented, so the
   retirement boundary is a caller-declared quiet interval (application policy recorded in the evidence) or a restart
   notification; until retired, a same-code reply is published as possibly late, never as a fresh confirmation. 12A Q9
   measures reply latency to inform the application's choice; the library invents no default.
3. Periodic reports 01H (V1.0.9 tables only): `[01, code, period ms]`, period 0 stops; setup reply `[01, code, status]`;
   then unsolicited `[code, value]` frames identical in form to the polled reply (MK109 p.36). Implement explicit
   `prepareReport` and `prepareReportStop` as settings operations (persistence unknown, treated as possibly retained):
   - only codes the ledger marks read-only and reportable; the manual's example is 31H, any other code stays unknown
     until 12A Q12 or a document confirms it;
   - while a stream for code c is active, or was active before a restart and has not since been stopped by an
     acknowledged period-0 request or declared off by the caller, polled reads of c on that address are refused before
     TX (03); stream samples carry `unsolicited` provenance, host receive time and no sequence;
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
   - broadcast ID 0 and group IDs (8DH) execute on every matching drive without reply (MK109 p.30-31, p.49, p.105);
     each member axis's execution becomes `UNKNOWN`, its pending phases and references are invalidated, and the
     operation names its members from the application's node table, which must keep group IDs disjoint from every
     slave ID; never used by discovery, probe, recovery or a common single-axis verb;
   - V1.0.0 and V1.0.1 still replied to broadcast and group frames (implied by the V1.0.2 note, MK109 p.1), on an
     undocumented identifier: refuse broadcast and group dispatch on `V1_0_0`.
7. Synchronization flag (V1.0.9 tables; since 1.0.8): 4AH enables latching (a setting through 08's context); a latched
   motion command answers 5 and waits for 4BH, sent once to ID 0 without reply (MK109 p.106-107). The library sends 4BH
   once per explicit request; the manual's repeat about every 1 ms is an application choice, and whether a second 4BH
   restarts or repeats a latched move is undocumented (MK-D12), so the operation records each 4BH as new intent. After
   4BH each member's execution is `UNKNOWN` until its own completion or observation. Native names only; never the
   CANopen `sync` verb, prompt 51's SYNC producer or any SYNC counter.
8. Silent mode (50H 02) is never sent; a drive left silent by another tool times out on everything and looks absent
   (MK-D32). Malformed frames are never sent deliberately outside an authorized 12A question (MK-D09).
9. Evidence classes for health (prompt 55): reply timeout, unmatched frame, possibly-late reply, checked rejection
   (status 0) and stream gap, per address. No CANopen heartbeat, EMCY or NMT state is synthesized. Retry classes come
   from the ledger; only reads under 01's decision may be repeated, as new operations within the deadline.

## Deliver

Extended matcher and stream table, `Reports.h/.cpp` with `ReportContext`, `Group.h/.cpp` (or the profile's
multi-target module) for broadcast, group and 4AH/4BH operations, `test/mks_servo_can_async_test.cpp`, owner-backed
app and console routes (`mks report`, `mks group`, `mks latch`, `mks execute-latched`), API/CLI parity, coverage and
user-guide sections on matching limits, report and PROGRESS row.

## Acceptance

Through the real owner and fake drive: stream frames interleaved with polled replies of other codes; polled read of a
streamed code refused; unsolicited 2/3 with and without a pending phase; late reply flagged until retirement;
respond-off and active-off policies; broadcast and group operations invalidating every member; 4BH sent once with no
reply and no repeat; V1_0_0 broadcast refused; a restart leaving previously active streams unknown until stopped or
declared off. The full matrix of contract 02 "Registration acceptance" for this major change and CI on the exact final
pushed commit pass. Hardware NOT RUN. Commit and push.
