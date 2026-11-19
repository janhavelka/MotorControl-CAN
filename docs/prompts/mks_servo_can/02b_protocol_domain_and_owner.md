# 02b — Protocol domain and owner admission

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 02. This stage registers the profile. Owner admission consumes seam S9 of
main-sequence [prompt 63](../63_mixed_protocols.md) with the MKS extension that prompt records under "Requests from
family promptsets (2026-11-19)", once it has landed and its checks passed, whatever that prompt's overall state
([shared README](../canopen_shared/README.md) "Consume"): its identifier plan, plan-matched receive route, raw
transmit kind and restart and bus-off notification, extended by an exclusive non-CANopen domain over the whole
standard-ID space on its own interface, stop precedence within that domain and a per-frame TX disposition. Apply
[canopen_shared 02](../canopen_shared/02_profile_integration_contract.md) "Common rules" for seams S1, S2 and S6 and its
"Other-protocol rules"; claim each seam in the shared README table before editing it, or extend it. Until the
extension lands, only owner admission and everything that transmits through the owner are BLOCKED; the inventory,
profile resolution, foreign-profile guards and host-only matcher logic still land and are tested.

## Work

1. Read S9 as landed: plan types, route callback, raw transmit admission class, stop precedence, TX disposition and
   restart or bus-off notification. Record in the report its landed commit, tested interface and the consumer tests
   this stage reruns, from the S9 row of the shared README table. Do not build a family-local owner, a second receive
   loop or a direct controller path from the profile.
2. **Exclusive domain.** Declare the MKS interface as one domain owning every standard data identifier 0x000-0x7FF:
   slave IDs 1..2047, broadcast 0 and group IDs, with replies on the request identifier (MK109 p.30-31, p.49). Refuse
   it when any CANopen node, route or COB-ID exists on that interface, and refuse CANopen admission on an MKS interface.
   Evidence for the refusal: 0x000 is CANopen NMT and MKS broadcast; 0x080 SYNC, 0x081-0x0FF EMCY, 0x181-0x57F PDOs,
   0x581-0x67F SDO and 0x701-0x77F heartbeat are valid MKS IDs ([principles pack](../../reference/canopen_principles/principles.md)
   section 2). Example (inference): an NMT Reset Communication `000: 82 00` reaches every MKS drive as a broadcast with
   code 82H (work mode) and DLC 2, whose handling is undocumented (MK-D09); a drive at 0x601 replies on what a CANopen
   node 1 reads as an SDO request.
3. Prompt 63's base plan serves a second protocol beside CANopen and refuses restricted-range plans (Implement 1 and
   3); the MKS extension it records ([README](README.md) open question 1) is what this stage consumes. Check that the
   landed extension provides, with its own tests: a whole-space MKS plan admitted only on an interface with no CANopen
   node, route or COB-ID, while shared-interface plans keep the restricted-range and overlap refusals; MKS stops
   (F7H forms, mode speed-0 stops) ahead of queued MKS ordinary frames; a per-frame TX disposition (not sent, sent,
   possibly sent) delivered for each admitted frame to the matcher of step 6; retirement of a queued or handed-off
   MKS frame with proof that it cannot transmit later (a final TX report or a successful retirement, as `TxReport` and
   `Transport::retire` in [CanBusTypes.h](../../../adapters/owner/CanBusTypes.h) give), or a disposition saying that a
   later transmission is not excluded (04 step 4); each routed frame with its capture time or bounds
   (`ReceivedFrame`); restart and bus-off notification to the MKS service. Rerun those consumer tests here. A need the landed artifacts do not cover keeps only its dependent
   owner admission Blocked, recorded in the report and PROGRESS against prompt 63's request. S9 is implemented only
   by executing prompt 63: no family-local substitute, weakened check or bypassed admission.
4. Seams S1 and S2 under contract 02 "Profile resolution": append `Manufacturer::MAKERBASE` and
   `DriveProfile::MKS_SERVO_CAN` (the next unused explicit nonzero value, never reused) to the inventory table with
   `Protocol::PROPRIETARY`, slave address bounds 1..2047, the per-dialect probe codes of 01 (40H for the V1.0.9
   tables) and model scopes from the ledger;
   `nonChanging`, `exactModel` and `firmware` stay false without evidence. `Endpoint.profile` holds that value, and the
   one pure `Discovery.h` lookup accepts an MKS binding only with `Endpoint.protocol` equal to `PROPRIETARY`. If MKS
   lands S2 first, the same commit carries the contract's Lichuan migration, after a read-only check of FieldCore-node's
   `Endpoint.profile` values ([audit application](../canopen_shared/audit_application.md), remaining items). Every MKS
   preparation refuses another family's binding and every Lichuan (and later family) preparation refuses an MKS
   binding, before touching output. Tests: the contract's list (migrated Lichuan binding with unchanged traffic, label
   41, unknown value and protocol mismatch refused, both cross-family directions, rebinding with a retained result),
   with nothing sent.
5. Declaration binding and probe. Each binding generation carries one validated `DialectDeclaration` (02): no
   default, declaring sends nothing, a probe or read without one returns `INVALID_CONFIG` before admission, and a
   changed declaration invalidates that binding's evidence, qualifications and pending work. The probe is 01's
   per-dialect probe read through the API, application, console (Blocked until S4) and fake drive: 40H for the V1.0.9
   tables, 01's chosen read for `V1_0_0`, which has no 40H. It proves a responder on that ID whose frame has the
   probe's code, an accepted length and a valid check byte under the declared dialect; not an MKS drive, model or
   firmware. That claim holds for a frame published `possiblyLate` (step 6), whose correlation it records, as a
   bootstrap observation that never stands as admission evidence for dependent work (03 step 5). Keep the
   raw bytes (MK-D05). Reads carry the read effect decided in 01; the probe never writes and, as a
   bootstrap read, consumes no identity (contract 02 "Identity and device evidence").
6. Matcher, owned by the MKS service context and fed by the S9 route:
   - key = interface and slave address; **one outstanding request per address**, because a 00H read reply carries the
     setting's code (MK109 p.37), a 01H report carries the read code (p.36) and run and stop share codes (MK-D36);
   - at most one pending completion phase per address, bound to the admitted motion's generation (used by 04-07);
   - match by identifier, echoed code, accepted length, check byte and phase, and only a frame captured (owner
     capture time) after the request's TX handoff whose disposition says sent or possibly sent; anything else is a
     copied raw unsolicited observation through the owner's observe path, never silently consumed and never
     attributed;
   - one immutable deadline per request (`now >= deadlineUs` expires), at most the operation's deadline (contract 02
     "Operation admission"); a missing write reply is execution UNKNOWN; a reply after the deadline is unattributable;
   - **ambiguous shapes.** The protocol has no transaction ID (review section 14 rule 2), so per address the matcher
     keeps the set of reply shapes an earlier exchange may still produce. A shape is the first data byte (echoed or
     reported code) with an accepted length, whatever request produced it: a write of setting x, a 00H read of x and
     a 01H sample of x can share one. A shape enters the set when an exchange that may have produced it ends, also
     one whose terminal frame arrived, because no reply is documented as unique and nothing excludes a delayed
     duplicate (review section 14 rule 2); and when a possibly sent exchange may produce another: deadline or
     timeout, local cancel, TX disposition possibly sent without a reply, a same-code stop (MK-D36, 04), an F5H update
     (05), a duplicate terminal frame, rebinding, controller restart or bus-off (the drive keeps running and may still
     send). At first attachment and after a host restart every shape of the address starts in the set, because a used
     drive may still complete work this host has no record of. So `QUALIFIED` correlation exists only for the first
     exchange of a shape after a recorded power cycle (below);
   - a frame of a shape in the set is published `possiblyLate`, never a fresh acknowledgement or completion, also
     while a later same-shape exchange is pending: an old status 2 arriving during a new same-code move ends nothing.
     The dependent stage then uses evidence that does not depend on attribution (05's endpoint check) or leaves the
     outcome `UNKNOWN`;
   - retirement: no reply latency is documented and a host controller restart does not stop the drive sending, so
     neither a host generation nor a restart retires a shape. The one evidenced boundary is a drive power cycle the
     caller records with its evidence reference, which empties the address's set and ends its identity session (03);
     shapes of power-on behaviors that may run afterwards (FFH's F6H, 9AH's homing) stay in the set. An application
     may declare a quiet interval as accepted residual timing risk: a same-shape frame captured once that interval
     has passed since the latest event that put the shape in the set is attributed with correlation
     `QUIET_INTERVAL` and the interval recorded, and the shape stays in the set. The interval is measured from the
     frame's owner capture time (`ReceivedFrame` time, or its earliest bound), not from when the service processes
     it, so a frame that waited in a host buffer is judged by its arrival; a frame without a capture time stays
     `possiblyLate`. `QUIET_INTERVAL` is a distinct risk-accepted outcome: it may end the exchange or phase it
     matches, its result carries that correlation and the interval, and it is never reported, counted or consumed as
     `QUALIFIED`. At attachment the application records one of the two, or every result stays `possiblyLate`. 12A Q9
     measures latency to inform that choice; the library invents no default;
   - restart and bus-off notification invalidate every MKS binding's evidence, pending phases and stream state.
7. Self-echo (MK-D35): the request `001: F3 01 F5` equals the success reply, so once delivered as ordinary RX the
   matcher cannot tell them apart. The exclusion is the transport boundary: `Transport::receive` must exclude local
   self-echo ([CanBusTypes.h](../../../adapters/owner/CanBusTypes.h)), and the TWAI adapter configures neither
   loopback nor self-test (`test/twai_driver_test.cpp`). Test the two separately: the fake transport never delivers
   its own TX (transport conformance), and the matcher accepts a byte-identical success frame only after the
   request's TX disposition, never one received before the handoff. A second host on the bus is unsupported and
   undetectable from the frames; document it in the coverage file and the user guide.
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

Contract 02 "Registration acceptance" and "Other-protocol rules". The S9 consumer tests of step 3 pass. Through the
real owner, S9 route, MKS service context and fake drive: probe success per dialect, missing declaration refused
before admission, wrong check byte, wrong length, wrong code, foreign profile both directions with no TX, reply on
broadcast or group ID ignored, timeout then late identical reply `possiblyLate`, a late reply arriving while a new
same-shape request is pending published `possiblyLate` and not as that request's reply, a completed exchange followed
by a same-shape request and the first delayed duplicate of the earlier reply (`possiblyLate` before the interval,
`QUIET_INTERVAL` after it, never `QUALIFIED`), the interval judged by capture time for a frame held in a host buffer,
every shape in the set at first attachment and emptied only by a recorded power cycle, a frame captured before the
request's handoff never matched, the fake transport never delivering its own TX, restart
notification invalidating evidence, and a CANopen node on the same interface refused in both directions. The S2 tests
pass; JSONL records follow S6 (console cases once S4 lands); checker negative tests fail as intended; all consumers
build in C++11. Quick and full verification, firmware builds, the family checks and CI on the exact final pushed
commit pass. Hardware NOT RUN. Commit and push.
