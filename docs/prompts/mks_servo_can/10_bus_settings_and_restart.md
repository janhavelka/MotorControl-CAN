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
   - 8DH group ID 0x01..0x7FF (p.49), validated disjoint from every slave ID in the application's node table;
   - 8CH reply policy (two bytes, one on V1_0_0) and 4CH check mode (MK109 dialect only, p.48).
2. Activation is undocumented: which identifier and bitrate carry the reply, and when the change applies (review
   section 14 rule 15). Each operation states old and new tuples, reserves both addresses, and accepts as acknowledged
   only a checked status reply on the old tuple; otherwise execution is `UNKNOWN`. Confirmation is a separate explicit
   read on the new tuple (`prepareCommunicationConfirmation`). Never scan guessed IDs or bitrates as recovery, never
   replay the write, and update the binding only after the caller accepts the confirmation.
   **Quarantine.** The old binding stays as the record but loses admission as soon as the write's TX disposition is
   sent or possibly sent, acknowledged or not, because the change may already apply. Scope: both tuples for 8BH, 8DH
   and 8CH; the whole interface for 8AH and 4CH. On entry, queued ordinary work on that scope is retired without TX
   and the scope's identity, read, enable, reference, stream, latch and completion qualifications are invalidated
   (AGENTS: configuration changes invalidate dependent evidence and queued work); a TX disposition of not sent leaves
   everything as it was. Only the confirmation read on the new tuple, a 00H read of the changed code on the old tuple
   where 03 established it readable, and an explicit operator reconciliation record (menu reading or node lost, step
   11) run in quarantine. It ends, and the reservations with it, only when the caller accepts one outcome that the
   evidence below supports: new tuple confirmed (new binding generation), old tuple confirmed unchanged (old binding
   restored with its evidence still invalidated), the setting's own unknown outcome where the table defines one, or
   the node recorded lost. A declaration change alone never ends quarantine, restores admission or sets group
   membership; the new declaration must match the accepted outcome. Because when a change applies is undocumented, a
   responder on the old tuple does not show the old value unchanged. The CAN menu shows CanRate, CanID, CanRSP
   (respond only) and CanCRC, and no group ID or active byte (MK109 p.25-26).

   | Write | Qualifying evidence | Unknown outcome |
   | --- | --- | --- |
   | 8BH slave ID, 8AH bitrate | New: a probe-grade responder (02b step 5) on the new tuple. Old: a 00H read of the code on the old tuple returning the old value (where 03 established it readable), or the operator's CanID or CanRate reading | None: unresolved means quarantine or node lost |
   | 8DH group ID | A 00H 8D read (where established readable) attributed `QUALIFIED` or `QUIET_INTERVAL` whose value is the old or the new ID; a response on the unchanged slave ID proves nothing about the group | "Group unknown": ordinary work on the slave ID resumes in a new binding generation, and the participant record holds both candidate group IDs, so group dispatch to either refuses until a deciding read |
   | 8CH reply policy | A deciding 00H 8C read where readable and answered; the operator's CanRSP reading settles the respond byte only | "Policy unknown": the binding runs under the weakest candidate, every command `AcknowledgementSupport::NONE` and ordinary reads unavailable (09 step 5), until a deciding read |
   | 4CH check mode | A probe-grade responder under the new mode, or the operator's CanCRC reading | None |
3. A bitrate change affects the whole interface: the host controller is reconfigured by the application at an idle,
   settled boundary, and every drive on that interface must share the new bitrate. Refuse the operation while other
   work on the interface is admitted.
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
   (menu access, step 11), whatever the prior reads.
8. 50H IAP (boot, silent, exit silent) and its RS485-framed broadcast examples (MK109 p.57-58, MK-D26) are never built.
9. Address planning helpers are host-side only: validate a proposed node table (unique slave IDs, disjoint group IDs,
   one bitrate and check mode per interface), reusing 09 step 6's participant validator for the exhaustive record it
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
is unavailable ending "policy unknown"; a declaration change alone refused as an outcome; a responder on the old tuple
not accepted as "old unchanged"; a proven unsent write keeping the binding; refused ID 0, duplicate IDs, overlapping
group IDs, 0x6B switch and admitted-work conflicts; restart and restore invalidating all evidence; 41H power-on motion
refusal unless accepted; 3FH refused with FFH and 9AH read off but without the explicit acceptance, safe-fixture record
and restore plan; no scan, retry or replay. The full matrix of contract 02 "Registration acceptance" for this major
change and CI on the exact final pushed commit pass. Restart-dependent hardware checks need actual restart control:
hardware NOT RUN. Commit and push.
