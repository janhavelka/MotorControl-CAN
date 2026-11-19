# 02b — Protocol domain and owner admission

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 02. This stage registers the profile. Owner admission consumes the artifacts of
main-sequence [prompt 63](../63_mixed_protocols.md) (seam S9) once they have landed and their checks passed, whatever
that prompt's overall state ([shared README](../canopen_shared/README.md) "Consume"): its identifier plan,
plan-matched receive route, raw transmit kind and restart and bus-off notification. Apply
[canopen_shared 02](../canopen_shared/02_profile_integration_contract.md) "Common rules" for seams S1, S2 and S6 and its
"Other-protocol rules"; claim each seam in the shared README table before editing it, or extend it. While those
artifacts are missing, only owner admission and everything that transmits through the owner are BLOCKED; the
inventory, profile resolution, foreign-profile guards and host-only matcher logic still land and are tested.

## Work

1. Read prompt 63 as landed: plan types, route callback, raw transmit admission class, TX evidence and restart or
   bus-off notification. Record the API in the report. Do not build a family-local owner, a second receive loop or a
   direct controller path from the profile.
2. **Exclusive domain.** Declare the MKS interface as one domain owning every standard data identifier 0x000-0x7FF:
   slave IDs 1..2047, broadcast 0 and group IDs, with replies on the request identifier (MK109 p.30-31, p.49). Refuse
   it when any CANopen node, route or COB-ID exists on that interface, and refuse CANopen admission on an MKS interface.
   Evidence for the refusal: 0x000 is CANopen NMT and MKS broadcast; 0x080 SYNC, 0x081-0x0FF EMCY, 0x181-0x57F PDOs,
   0x581-0x67F SDO and 0x701-0x77F heartbeat are valid MKS IDs ([principles pack](../../reference/canopen_principles/principles.md)
   section 2). Example (inference): an NMT Reset Communication `000: 82 00` reaches every MKS drive as a broadcast with
   code 82H (work mode) and DLC 2, whose handling is undocumented (MK-D09); a drive at 0x601 replies on what a CANopen
   node 1 reads as an SDO request.
3. Prompt 63 is written for a second protocol beside CANopen: plans are checked against configured CANopen
   identifiers and the restricted ranges, and its raw kind has one admission class below stops with local TX evidence
   only (prompt 63 Implement 1 and 3). MKS needs a whole-space plan on an interface without CANopen, an urgent class
   for its own stops (F7H, mode speed-0 stops) above its ordinary requests, and per-frame TX evidence (sent, not sent,
   possibly sent) for the matcher of step 6. Where the landed artifacts cannot express one of these, record it in the
   report and PROGRESS as a request to prompt 63's owner ([README](README.md) open question 1) and keep the dependent
   owner admission Blocked. S9 is implemented only by executing prompt 63 as written: no family-local substitute,
   weakened check or bypassed admission.
4. Seams S1 and S2 under contract 02 "Profile resolution": append `Manufacturer::MAKERBASE` and
   `DriveProfile::MKS_SERVO_CAN` (the next unused explicit nonzero value, never reused) to the inventory table with
   `Protocol::PROPRIETARY`, slave address bounds 1..2047, probe code 40H and model scopes from the ledger;
   `nonChanging`, `exactModel` and `firmware` stay false without evidence. `Endpoint.profile` holds that value, and the
   one pure `Discovery.h` lookup accepts an MKS binding only with `Endpoint.protocol` equal to `PROPRIETARY`. If MKS
   lands S2 first, the same commit carries the contract's Lichuan migration, after a read-only check of FieldCore-node's
   `Endpoint.profile` values ([audit application](../canopen_shared/audit_application.md), remaining items). Every MKS
   preparation refuses another family's binding and every Lichuan (and later family) preparation refuses an MKS
   binding, before touching output. Tests: the contract's list (migrated Lichuan binding with unchanged traffic, label
   41, unknown value and protocol mismatch refused, both cross-family directions, rebinding with a retained result),
   with nothing sent.
5. Probe: one 40H read through the API, application, console (Blocked until S4) and fake drive. It proves a responder
   on that ID whose reply has code 40H, an accepted length and a valid check byte under the declared dialect; not an
   MKS drive, model or firmware. Keep the raw bytes (MK-D05). `V1_0_0` has no documented 40H, so its probe is the
   ledger's chosen V1.0.0 read (for example 30H), decided in 01. Reads carry the read effect decided in 01; the probe
   never writes and, as a bootstrap read, consumes no identity (contract 02 "Identity and device evidence").
6. Matcher, owned by the MKS service context and fed by the 63 route:
   - key = interface and slave address; **one outstanding request per address**, because a 00H read reply carries the
     setting's code (MK109 p.37), a 01H report carries the read code (p.36) and run and stop share codes (MK-D36);
   - at most one pending completion phase per address, bound to the admitted motion's generation (used by 04-07);
   - match by identifier, echoed code, accepted length, check byte and phase; anything else is a copied raw
     unsolicited observation through the owner's observe path, never silently consumed and never attributed;
   - one immutable deadline per request (`now >= deadlineUs` expires); a missing write reply is execution UNKNOWN; a
     reply after the deadline is unattributable, and the address is flagged `possiblyLate` for that code until 09's
     retirement rule clears it;
   - restart and bus-off notification invalidate every MKS binding's evidence, pending phases and stream state.
7. Self-echo (MK-D35): the request `001: F3 01 F5` equals the success reply. Rely on the transport contract that
   received frames exclude local self-echo (`Transport::receive` in [CanBusTypes.h](../../../adapters/owner/CanBusTypes.h))
   and add a test that an echoed request never becomes an acknowledgement. A second host on the bus is unsupported
   and undetectable from the frames; document it in the coverage file and the user guide.
8. Seam S6 and console under contract 02 "Console, help and JSONL": one `mks` entry in
   `examples/common/ProbeConsole.cpp`, parser and formatter in `examples/common/MksServoCanConsole.{h,cpp}`,
   `mks probe` as the first route, all Blocked until S4 lands (README open question 4). The S6 record rule is landed or
   extended here with the schema in `docs/cli_contract.md`, `scripts/console_jsonl.py`, `scripts/s2_console.py` and
   their tests: `profile` is `null` without a target or for an unresolved target, `"mks_servo_can"` on targeted MKS
   replies and refusals, and the admission value on operation records and retained results, also after a rebind,
   checked against the request correlated by `id`. `nmt`, `heartbeat`, `network`, `sync` and `pdo` refuse an MKS
   binding before TX. Existing top-level verbs keep their Lichuan meaning.
9. Fake drive `test/support/MksServoCanDrive.h` on the owner `Transport` callbacks: documented frames only, labelled
   synthetic; it can withhold, delay, duplicate or alter replies and emit unsolicited 2/3 and 01H frames.

## Deliver

Inventory entry and guards, `Service.h` (or the profile's matcher module) and sources, the probe route, console
family (Blocked until S4), fake drive, `test/mks_servo_can_owner_test.cpp`, `test/mks_servo_can_app_test.cpp` and
extended `test/multidrive_contract_test.cpp` cases; S1, S2 and S6 claims and landed commits in the canopen_shared
README table; coverage, CLI contract, API/CLI map and source map entries for the probe; the prompt 63 request where
needed; report and PROGRESS row.

## Acceptance

Contract 02 "Registration acceptance" and "Other-protocol rules". Through the real owner, prompt-63 route, MKS
service context and fake drive: probe success, wrong check byte, wrong length, wrong code, foreign profile both
directions with no TX, reply on broadcast or group ID ignored, timeout then late identical reply flagged
`possiblyLate`, echoed request not acknowledged, restart notification invalidating evidence, and a CANopen node on the
same interface refused in both directions. The S2 tests pass; JSONL records follow S6 (console cases once S4 lands);
checker negative tests fail as intended; all consumers build in C++11. Quick and full verification, firmware builds,
the family checks and CI on the exact final pushed commit pass. Hardware NOT RUN. Commit and push.
