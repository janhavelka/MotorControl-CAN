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
3. A bitrate change affects the whole interface: the host controller is reconfigured by the application at an idle,
   settled boundary, and every drive on that interface must share the new bitrate. Refuse the operation while other
   work on the interface is admitted.
4. Check mode: after 4CH the host builds with the new mode, but the uplink check in fixed-0x6B mode is undocumented
   (MK-D08), so switching to 0x6B is refused until 12A Q5 resolves it. MK109A's 0FH is never sent.
5. Reply policy: setting respond 0 makes later commands unacknowledged and possibly reads unavailable (MK-D11), so the
   confirmation of its own effect may be impossible; the operation ends acknowledged-only on the old policy's reply and
   the binding's declaration changes only by explicit caller action.
6. Restart 41H ("does not modify configuration parameters", MK109 p.56): `explicit_restart`, never automatic. Its
   status reply may or may not precede the restart (undocumented). Afterwards enable state (MK-D13), coordinate origin
   (inference: volatile), streams, latches and completion phases are unknown, and power-on behaviors may run: FFH
   auto-run and 9AH homing move the shaft at power-on (p.27, p.85); whether a 41H restart triggers them is
   undocumented. Refuse 41H unless FFH and 9AH are read as off or the caller explicitly accepts power-on motion.
7. Factory restore 3FH restarts the drive; since 1.0.8 no recalibration is needed (MK109 p.1, p.56; MK106 p.33 required
   it). The restored scope (CAN ID, bitrate, check mode, group ID, reply policy, FFH, 9AH) is not listed (MK-D22), so
   communication after restore is `UNKNOWN` and every binding fact is invalidated: `prepareFactoryRestore` with an
   unknown communication outcome, explicit intent and the same power-on motion refusal as 41H.
8. 50H IAP (boot, silent, exit silent) and its RS485-framed broadcast examples (MK109 p.57-58, MK-D26) are never built.
9. Address planning helpers are host-side only: validate a proposed node table (unique slave IDs, disjoint group IDs,
   one bitrate and check mode per interface). Two drives on one ID corrupt every exchange on it; the library cannot
   detect an unknown drive safely and performs no ID sweep.
10. Invalidation: a confirmed ID change ends the old binding generation and starts a new one; a group-ID change
    invalidates multi-target member lists (09); a bitrate or check-mode change invalidates every binding on the
    interface until each is confirmed again.
11. The commissioning guide documents the menu equivalents (CanRate, CanID, CanRSP, CanCRC; MK109 p.25-26) as the
    recovery path when a node is lost, and the Next-key power-on restore (p.28, p.115). The library documents these
    operator actions and cannot perform them.

## Deliver

`Communication.h/.cpp`, `CommunicationContext`, restart and restore preparations (`Persistence.h/.cpp` if separate),
`test/mks_servo_can_communication_test.cpp`, owner-backed app and console routes (`mks bitrate`, `mks id`,
`mks group-id`, `mks reply-policy`, `mks restart`, `mks factory-restore`), API/CLI parity, the commissioning guide
section, coverage rows, report and PROGRESS row.

## Acceptance

Old/new tuple handling; reply on old tuple, missing reply and reply on an unexpected identifier; confirmation as a
separate read; refused ID 0, duplicate IDs, overlapping group IDs, 0x6B switch and admitted-work conflicts; restart
and restore invalidating all evidence; power-on motion refusal unless accepted; no scan, retry or replay. The full
matrix of contract 02 "Registration acceptance" for this major change and CI on the exact final pushed commit pass.
Restart-dependent hardware checks need actual restart control: hardware NOT RUN. Commit and push.
