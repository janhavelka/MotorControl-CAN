# 10 — Bus settings, restart and factory restore

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 03, 08 and 09. MK-D08, D13, D20 and D22 are central. Every operation here can lose the
node; none is used for discovery, probe or recovery. While COMMON records the owner as away, communication changes are
forbidden on any bench.

## Work

1. Typed bus settings through 08's context, with `prepareCommunication`/`nextCommunication`/`advanceCommunication`:
   - 8AH bitrate codes 0 125k, 1 250k, 2 500k (default), 3 1M (MK109 p.25, p.46); sample point and timing undocumented;
   - 8BH slave ID uint16 (p.46); accept 1..2047 and refuse 0 (broadcast). The menu covers 1..16 and Part 4 says
     "01~10", consistent only in hexadecimal (MK-D20). Older CAN specifications reserved identifiers whose seven most
     significant bits are all recessive (0x7F0-0x7FF); assign those only when the host controller's documentation or a
     measurement admits them;
   - 8DH group ID 0x01..0x7FF (p.49), validated disjoint from every slave ID in the application's physical
     participant list (02b step 5), not only its configured bindings;
   - 8CH reply policy (two bytes, one on V1_0_0) and 4CH check mode (MK109 dialect only, p.48).
2. Activation is undocumented: which identifier and bitrate carry the reply, and when the change applies (review
   section 14 rule 15). Each operation states old and new tuples, reserves both addresses, and accepts as acknowledged
   only a checked status reply on the old tuple; otherwise execution is `UNKNOWN`. Confirmation is a separate explicit
   read on the new tuple (`prepareCommunicationConfirmation`). Never scan guessed IDs or bitrates as recovery, never
   replay the write, and update the binding only after the caller accepts the confirmation.
   **Quarantine.** The fence starts when the write is admitted, before its handoff: from then the scope admits only
   the reads and records named below, and queued ordinary work on it is retired without TX. Scope: both tuples for
   8BH, 8DH and 8CH; the whole interface for 8AH, 4CH and 3FH (step 7). An interface-scope write is admitted only
   when, checked atomically at its admission, no address on the interface holds any 04 step 4 motion-record condition
   (live intent, calibration, open TX, uncertain execution, retained obligation, lost history) or a queued or
   handed-off trigger; otherwise it refuses before TX, because the fence would strand that address's stop path.
   Stops to every address stay admitted throughout any quarantine, not only after node lost (04 step 2), each result
   naming the active fence. A TX disposition proving the write unsent
   and unable to transmit later (02b step 3) lifts the fence with everything as it was. Once the disposition is sent
   or possibly sent, acknowledged or not, the change may already apply: the old binding stays as the record without
   admission, and the scope's identity, read, enable, reference, stream, latch and completion qualifications are
   invalidated (AGENTS: configuration changes invalidate dependent evidence and queued work). Only the table's
   confirmation reads on the new tuple, a 00H read of the changed code on the old tuple where 03 established it
   readable, and an explicit operator reconciliation record (menu reading or node lost, step 11) run in quarantine.
   It ends, and the reservations with it, only when the caller accepts one outcome that the evidence below supports,
   taken after these settlements:
   - TX settlement: the write frame is settled (04 step 4 discharge ordering, deciding reads). A deciding read or
     operator reading counts only when taken after it, because a frame held below the owner queue could apply after
     the reading.
   - Configuration settlement: when a change applies is undocumented (review section 12 and section 14 rule 15), so a
     value equal to the old one shows the current setting, not that a received change will not apply later. An
     outcome resting on the old value (old tuple unchanged, or the old candidate of an 8DH or 8CH deciding read)
     also needs, after TX settlement, a recorded drive power cycle (02b step 6) before its deciding read (inference:
     stored settings apply at power-on), or the operator's reconciliation record on this specimen accepting the risk
     of a deferred application, recorded in the outcome. A reading of the new value shows the change applied and
     needs TX settlement only.

   Outcomes: new tuple confirmed (new binding generation), old tuple confirmed unchanged (old binding restored with
   its evidence still invalidated), the setting's own unknown outcome where the table defines one (built to tolerate a
   late application, so it needs no settlement), or the node recorded lost. Node lost ends the binding, not the
   reservations: every candidate tuple stays reserved, so no binding, probe or ordinary frame uses it; the drive stays
   in the participant record with every candidate value, so multi-target dispatch and 4BH on the interface refuse
   (09 step 6); and after an 8AH write or 3FH the interface's other bindings stay fenced for changing and
   multi-target traffic (inference: a drive at another bitrate disturbs every frame it sees), stops staying admitted;
   each until an operator reconciliation record states the drive's configuration or its removal from the bus. Reads
   to other addresses stay admitted there with their replies reported unqualified, never admission evidence, because
   the lost node may answer on a reused ID (round 4 lead decision 4), but only to an address with recipient
   isolation: the operator record, the physical participant list (02b step 5) or the exhaustive participant record
   (09 step 6) shows that no lost or uncommissioned participant can hold that address as its slave or group ID, or the ledger reviews the read's
   request and reply as non-changing for every candidate recipient. Otherwise the read refuses before TX: a drive
   whose group ID equals the address takes the reply as a command (review section 14 rule 5), and a 00H read reply
   `[code, value]` or a 42H reply has the layout of that code's write (MK109 p.37, p.39). The same isolation applies
   wherever a participant's group ID is `UNKNOWN` or a candidate set (02b step 5). A declaration change alone never
   ends quarantine, restores admission or sets group membership; the new declaration must match the accepted
   outcome, using 02 step 1's `UNKNOWN` or candidate-set values for an unknown outcome. Because when a change applies is
   undocumented, a responder on the old tuple does not show the old value unchanged. The CAN menu shows CanRate,
   CanID, CanRSP (respond only) and CanCRC, and no group ID or active byte (MK109 p.25-26).

   | Write | Qualifying evidence | Unknown outcome |
   | --- | --- | --- |
   | 8BH slave ID, 8AH bitrate | New: a read on the new tuple attributed `QUALIFIED` or `QUIET_INTERVAL` (02b step 6; a probe-grade or `possiblyLate` responder only shows that something answers), plus continuity with the intended drive: a 42H read equal to the user ID read from this drive before the write and unique in the participant record, or the operator's CanID or CanRate reading on this specimen. Old: a 00H read of the code on the old tuple, attributed the same way, returning the old value (where 03 established it readable), or the operator's CanID or CanRate reading | None: unresolved means quarantine or node lost |
   | 8DH group ID | A 00H 8D read (where established readable) attributed `QUALIFIED` or `QUIET_INTERVAL` whose value is the old or the new ID; a response on the unchanged slave ID proves nothing about the group | "Group unknown": ordinary work on the slave ID resumes in a new binding generation declared with both candidate group IDs (02 step 1 candidate set, 02b step 5), and the participant record holds both, so group dispatch to either refuses until a deciding read |
   | 8CH reply policy | A deciding 00H 8C read where readable and answered; the operator's CanRSP reading settles the respond byte only | "Policy unknown": the binding is declared with the unsettled bytes `UNKNOWN` (02 step 1) and runs under the weakest candidate, every command `AcknowledgementSupport::NONE` and ordinary reads unavailable (09 step 5, 02b step 5), until a deciding read |
   | 4CH check mode | The operator's CanCRC reading, or a reply on the unchanged slave ID attributed `QUALIFIED` or `QUIET_INTERVAL` whose check byte is valid under one mode only, once 12A Q5 has established the fixed mode's uplink check (MK-D08); a reply valid under both (its 8-bit sum is 0x6B) or a probe-grade responder decides nothing | None |
3. A bitrate change affects the whole interface: the host controller is reconfigured by the application at an idle,
   settled boundary, and every drive on that interface must share the new bitrate. Refuse the operation while other
   work on the interface is admitted (step 2's interface-scope check). One 8AH changes one drive, and an idle host
   queue does not change the other drives' bitrate: admit it only when the exhaustive participant record (09 step 6)
   lists no other powered participant on the interface, or the application's recorded plan names how every other
   participant changes (each by its own 8AH under this quarantine) and accepts the mixed-bitrate interval.
4. Check mode: after 4CH the host builds with the new mode, but the uplink check in fixed-0x6B mode is undocumented
   (MK-D08), so switching to 0x6B is refused until 12A Q5 resolves it. MK109A's 0FH is never sent.
5. Reply policy: setting respond 0 makes later commands unacknowledged and possibly reads unavailable (MK-D11), so the
   confirmation of its own effect may be impossible; the operation ends acknowledged-only on the old policy's reply,
   and the address stays in step 2's quarantine until the caller accepts an outcome step 2's 8CH row supports
   ("policy unknown" when no deciding read exists). The binding's declaration then changes only by explicit caller
   action matching that outcome.
6. Restart 41H ("does not modify configuration parameters", MK109 p.56): `explicit_restart`, never automatic. Its
   status reply may or may not precede the restart (undocumented). Afterwards enable state (MK-D13), coordinate origin
   (inference: volatile), streams, latches and completion phases are unknown, and power-on behaviors may run: FFH
   auto-run and 9AH homing move the shaft at power-on (p.27, p.85); whether a 41H restart triggers them is
   undocumented. Refuse 41H unless FFH and 9AH are read as off or the caller explicitly accepts power-on motion.
7. Factory restore 3FH restarts the drive; since 1.0.8 no recalibration is needed (MK109 p.1, p.56; MK106 p.33 required
   it). The restored scope (CAN ID, bitrate, check mode, group ID, reply policy, FFH, 9AH) is not listed (MK-D22), so
   communication after restore is `UNKNOWN` and every binding fact is invalidated: `prepareFactoryRestore` with an
   unknown communication outcome and explicit intent. Its gate differs from 41H's: FFH and 9AH read off before 3FH
   do not establish their values when the drive restarts, because the restore may change them and FFH's default is
   not documented (MK109 p.85; 0_Mode defaults to Disable, p.27). Unless evidence for this specimen and dialect
   establishes the post-restore power-on behavior, refuse 3FH without the caller's explicit acceptance of possible
   power-on motion, a recorded physically safe fixture (free shaft, nothing attached) and a feasible restore plan
   (menu access, step 11), whatever the prior reads. 3FH enters step 2's quarantine with the interface as scope. Its
   candidate tuples are the old one and the menu defaults (CanRate 500K, CanID 01, CanRSP Enable, CanCRC SUM-8;
   MK109 p.25-26), each reserved, so 3FH is refused while another participant's slave or group ID equals a candidate
   ID (two drives on one ID corrupt every exchange, step 9). The participant record holds the old and default values,
   with the group ID and the 8CH active byte unknown, so multi-target dispatch and 4BH on the interface refuse until
   reconciliation, and frames to any other address need step 2's recipient isolation. Quarantine ends only through step 2's settlements and table rows for each field the restore may
   have changed (8BH and 8AH on one candidate tuple, 4CH, 8CH, 8DH or their unknown outcomes) or node lost; a reply on
   a candidate or unexpected tuple admits nothing by itself.
8. 50H IAP (boot, silent, exit silent) and its RS485-framed broadcast examples (MK109 p.57-58, MK-D26) are never built.
9. Address planning helpers are host-side only: validate a proposed node table against the physical participant
   list (02b step 5; unique slave IDs, disjoint group IDs, one bitrate and check mode per interface), reusing 09 step 6's participant validator for the exhaustive record it
   requires before any broadcast or group dispatch. Two drives on one ID corrupt every exchange on it; the library
   cannot detect an unknown drive safely and performs no ID sweep, so exhaustiveness is the application's commissioning
   record.
10. Invalidation: a confirmed ID change ends the old binding generation and starts a new one; a group-ID change
    invalidates multi-target member lists (09); a bitrate or check-mode change invalidates every binding on the
    interface until each is confirmed again.
11. The commissioning guide documents the menu equivalents (CanRate, CanID, CanRSP, CanCRC; MK109 p.25-26) as the
    recovery path when a node is lost, and the Next-key power-on restore (p.28, p.115). The library documents these
    operator actions and cannot perform them.

## Deliver

`Communication.h/.cpp`, `CommunicationContext`, restart and restore preparations (`Persistence.h/.cpp` if separate),
`test/mks_servo_can_communication_test.cpp`, owner-backed app and console routes (`mks bitrate`, `mks id`, `mks
group-id`, `mks reply-policy`, `mks restart`, `mks factory-restore`), API/CLI parity (console routes and parity once S4
lands), the commissioning guide section, coverage rows, report and PROGRESS row.

## Acceptance

Old/new tuple handling; reply on old tuple, missing reply and reply on an unexpected identifier; confirmation as a
separate read; a lost reply where the fake drive applied the change and where it did not, each followed by an ordinary
operation on both tuples (refused in quarantine), then each reconciliation outcome; an applied and an unapplied 8DH
write with identical probe results, both ending "group unknown" without a deciding read; an 8CH write whose confirmation
is unavailable ending "policy unknown"; a `possiblyLate` probe reply on the new tuple, an unrelated responder there
(42H differing from the pre-write read) and a 4CH reply valid under both check modes each leaving quarantine in place;
a declaration change alone refused as an outcome; a responder on the old tuple
not accepted as "old unchanged"; a proven unsent write keeping the binding; no ordinary frame on the scope admitted
between the write's admission and its disposition; a write held below the owner queue without a final report, an
old-value read before its settlement, then the late TX applying the change: quarantine kept and no ordinary or
multi-target TX; an old-value read after TX settlement but without a recorded power cycle or the operator's risk
record not accepted as "old unchanged"; node lost after an 8AH write keeping both tuples reserved, the interface's
other bindings refusing changing work and multi-target dispatch refused until the operator record, while a stop to
another address stays admitted and a read there is admitted with its reply reported unqualified only with recipient
isolation established; a restored drive whose group ID may equal another slave ID, and a 42H or 00H read to that
slave refused before TX without isolation (no frame that the lost drive could take as a write); a timed run on
address B with a 4CH or 3FH request on A just before B's stop due refused before TX, and with B idle the write
admitted while a stop to B stays admitted during the fence; an 8AH refused with another powered participant and no
recorded plan; a "group unknown" and a "policy unknown" outcome expressed by the new declaration's candidate set or
`UNKNOWN` values; 3FH entering interface quarantine
with the default candidate tuples, refused while another participant uses ID 01, and a restore answering on an
unexpected tuple admitting no ordinary or multi-target TX; refused ID 0, duplicate IDs, overlapping
group IDs, 0x6B switch and admitted-work conflicts; restart and restore invalidating all evidence; 41H power-on motion
refusal unless accepted; 3FH refused with FFH and 9AH read off but without the explicit acceptance, safe-fixture record
and restore plan; no scan, retry or replay. The full matrix of contract 02 "Registration acceptance" for this major
change and CI on the exact final pushed commit pass. Restart-dependent hardware checks need actual restart control:
hardware NOT RUN. Commit and push.
