# Execution contract for every MKS SERVO42D/57D CAN stage

Read root [AGENTS.md](../../../AGENTS.md), [COMMON.md](../COMMON.md) (including the owner-absence section),
[CONVENTIONS.md](../CONVENTIONS.md), [DOCUMENTATION.md](../DOCUMENTATION.md), [MAINTAINABILITY.md](../MAINTAINABILITY.md),
[DELEGATION.md](../DELEGATION.md), [MULTIDRIVE.md](../MULTIDRIVE.md), [ADD_DRIVE.md](../ADD_DRIVE.md) "For another CAN
protocol", the [multi-drive decision](../../multidrive_decision.md), [other CAN protocols](../../audit/OTHER_CAN_PROTOCOLS.md)
"Required architecture consequences", [prompt 63](../63_mixed_protocols.md), the [promptset index](README.md), the
[reference pack](../../reference/mks_servo_can/README.md) and [review](../../reference/mks_servo_can/review.md), then
the selected stage. Apply the [canopen_shared](../canopen_shared/README.md) set at `a7656efb` or later: its README
"Seams: claim, land, consume", [01](../canopen_shared/01_existing_code_and_tooling.md) "Intake rules" and "A non-CANopen
CAN protocol instead", and [02](../canopen_shared/02_profile_integration_contract.md) "Common rules" and "Other-protocol
rules"; its "CANopen-only rules" do not apply. Cite them, do not restate them. The README lists this set's open
questions against them. Original PDF pages are authoritative for what is documented; firmware qualification is
independent.

## Repository and consumer boundaries

- Work on `main` under the current Git instruction (no new branches until version0). Only the lead fetches, commits and
  pushes; preserve concurrent work and stage owned paths only. Subagents take disjoint paths or read-only reviews.
- Refresh MotorControl-RS HEAD and its profile seam per AGENTS' recurring audit; inspect FieldCore-node read-only. Edit
  no sibling repository. Do not mark main-sequence prompts complete as part of this family.
- Layout from contract 02: `include/MotorControlCAN/profiles/mks_servo_can/`, `src/profiles/mks_servo_can/`, namespace
  `MotorControlCAN::MKS_SERVO_CAN`, `DriveProfile::MKS_SERVO_CAN`, `Manufacturer::MAKERBASE` (MKS is Makerbase's brand),
  inventory `profileId` and JSONL `profile` `mks_servo_can`, family files `mks_servo_can_*`. Stage 01 confirms the
  names; a landed name changes only with a migration entry.
- Installed core stays C++11, heap-free, framework-independent and I/O-free: free functions, fixed value types,
  caller-owned contexts with `prepare...`/`next...`/`advance...` and checked getters, atomic outputs, borrowed input.
  Applications own the controller, time, scheduling, queues, caches, health policy and retries.
- Reuse only what canopen_shared 01 "A non-CANopen CAN protocol instead" lists: `Status`, `Endpoint` with
  `Protocol::PROPRIETARY`, `Capabilities`, units and axis preparation, `Native.h`, the operation vocabularies
  (`checkServiceEvidence` with `AcknowledgementSupport::NONE` where no reply exists), `can/Frame.h` and the owner
  `Transport`. Under contract 02 "Other-protocol rules" the family's own codecs and service context run beneath common
  motion through the real `CanBusOwner` once S9 with the MKS extension prompt 63 records ([README](README.md) open
  question 1) has landed and passed; never `canopen/`, `SdoClient`, `SdoReuse`, NMT, heartbeat, PDO, SYNC, a
  statusword projection or the CANopen console verbs. A missing native service, owner route or probe is Blocked, never
  routed through SDO.
- Seams: claim S1, S2, S6 or S7 in the canopen_shared README table before a shared edit, or extend the one already
  claimed or landed; consume S4, S5 and S9 as landed artifacts whose checks passed. Every `mks` console route and
  API/CLI parity check named in 02b-10 is Blocked until S4 lands (contract 02 "Console, help and JSONL"; README open
  question 4); the API, application and owner-backed tests of the same stage proceed.

## Evidence and supported behavior

- Cite the source ID and one-based PDF page (MK109 p.N). Reopen the rendered page for every table, layout, DLC and
  example used as code; vendor DLC columns and several worked examples are wrong (MK-D04..D07). Vendor examples are
  fixtures; a corrected frame is labelled derived, never a repaired snapshot. Keep originals and hashes unchanged.
- The command ledger created in stage 01 is the single source for codes, layouts, dialect rows, persistence, side
  effects and gates. Carry MK-D01..MK-D33 and the README's MK-D34..D38 into it. Close a discrepancy only with cited new
  evidence: a document, or a retained trace from an identified board and firmware.
- No generic assumption: no CiA 402 or CANopen meaning, no SERVO42C/RS485/Modbus behavior, no ODrive or VESC rule.
  Third-party MKS libraries corroborate frame mechanics only, never firmware behavior.
- A command absent from the declared dialect, a reserved bit pattern, an out-of-range field or a capability the
  evidence does not support returns `UNSUPPORTED` or `ILLEGAL_VALUE` before admission, with no output change.
- **Treat every configuration write as persistent** until a stage measures otherwise; only 83H's trailing-00 form
  (MK109 p.42) and MK109A's never-written 0FH (MK109A p.45) document a volatile form, and the menu says settings save
  automatically (MK109 p.9, MK-D21). That handling is policy: a command whose persistence is not stated (42H, 98H,
  01H, 4AH, 8CH and others, review section 11) stays `unknown` in the ledger, never documented. No cyclic or
  test-loop writes.
- **Never written by the library:** FFH power-on auto-run, 9AH power-on single-turn homing, 50H IAP/silent, MK109A's 0FH
  no-verification mode. Their state is read and reported where 00H allows. A later disabling write needs an explicit
  owner decision and its own stage amendment.

## Dialects and identity

- An explicit `DialectDeclaration` per binding generation selects the table, check mode, uplink check, reply policy
  (8CH) and group ID. There is no default and selection performs no I/O. A version label, a 40H reply or a successful
  read never selects or changes it.
- 40H reports calibration, board code, raw firmware bytes and bootloader interface (MK109 p.38); it cannot distinguish
  MK109 from MK109A (MK-D01) and its firmware field is undecoded (MK-D05). Under contract 02 "Identity and device
  evidence" each write or motion's coverage row names the identity it consumes by ledger key, its holder and its
  invalidators: the product and interface identity of 40H (board code 2 or 4, bootloader 0x20). A missing, stale or
  contradicting identity (RS485 board code, bootloader 0x10, board 6 or 8) refuses dependent work with nothing sent;
  the probe and identity reads need none. Firmware is not consumed, as in
  [prompt 57](../57_regular_drive_operations.md)'s identity policy: its raw bytes and the declared dialect are reported
  in capability output and terminal results as unqualified, never used as a lock. `V1_0_0` identity is
  [README](README.md) open question 3.
- No universal probe and no ID sweep. A checked reply proves a responder on that identifier with a matching check byte,
  not an MKS drive, a model or a firmware. Keep binding explicit, as [backlog D18](../../backlog.md) requires for
  CANopen identity collisions.

## Protocol domain, owner and bus rules

- One owner per physical interface. **One exclusive MKS domain per interface:** contract 02 "Multi-profile consumers
  and package scope" puts a Lichuan axis and a family axis on separate interfaces by default and refuses an unanalysed
  second domain on one interface; for MKS the separate interface is the only arrangement. The drives use the whole
  11-bit space (slave 1..2047, broadcast 0, group IDs) and reply on the request identifier (MK109 p.30-31, p.49). ID
  0x000 is also CANopen NMT and 0x080-0x0FF, 0x181-0x67F and 0x701-0x77F are CANopen identifiers that are valid MKS IDs
  ([principles pack](../../reference/canopen_principles/principles.md) section 2). Never share a bus with CANopen; no
  shared-interface positive test is planned.
- Exactly one host on the bus, with the transport's self-echo exclusion (`Transport::receive` in
  [CanBusTypes.h](../../../adapters/owner/CanBusTypes.h)); an echoed request can equal a success reply (MK-D35).
- One outstanding request per drive address, plus at most one pending completion phase bound to the admitted motion.
  A stop takes that slot only after the outstanding request is retired or ended ([04](04_enable_stop_and_uncertainty.md)
  step 4). A frame whose shape an earlier exchange may still produce is never a fresh acknowledgement or completion
  ([02b](02b_protocol_domain_and_owner.md) step 6). Group IDs stay disjoint from every slave ID (review section 14
  rule 5), checked over the application's physical participant list (02b step 5).
- No implicit enable, motion, homing, stall release, zero set, mode change, current change, ID/bitrate/check/reply
  change, synchronization flag, host-silence protection, restart or restore on startup, selection, probe, recovery or
  cached reads. Local cancel is not a stop. Controller TX, CAN ACK (every powered drive ACKs, MKSCH42/MKSCH57) and an
  immediate status 1 are different from completion.
- No automatic replay of motion, enable, homing, settings or 4BH. The manual's 1 ms 4BH repeat (MK109 p.107) is an
  application choice with MK-D12 unresolved.
- 98H host-silence protection is a drive setting the application may enable explicitly; never a library default, never
  fed implicitly; what feeds it is unknown (MK-D18). It is not a CANopen heartbeat.
- Do not fake CANopen state, NMT, SDO acknowledgements, heartbeat, EMCY or a statusword. `nmt`, `heartbeat`, `network`,
  `sync` and `pdo` routes refuse MKS bindings before TX; MKS "synchronization" (4AH/4BH) is a native feature.

## Verification and delivery

Inspect workflows, pins and registrations first; start with focused tests of changed behavior and failure paths.

```sh
python scripts/check_promptset.py
python scripts/check_promptset.py --check-git-ref INDEX
python scripts/check_mks_servo_can_ledger.py
python scripts/generate_mks_servo_can_commands.py --check
python scripts/check_mks_servo_can_operations.py
python scripts/verify.py --mode quick
python scripts/verify.py --mode full
python scripts/check_firmware.py --target arduino
python scripts/check_firmware.py --target idf
git diff --check
```

The three family checks are created in stage 01. Use `--check-git-ref INDEX` (then `HEAD` after commit) when reference
artifacts or `.gitattributes` change. Firmware targets follow [verification.md](../../verification.md). Contract 02
"Registration acceptance" applies at 02b, which registers the profile, and again at each later major change under
AGENTS (public API or behavior, owner or protocol integration, persistence, build, toolchain or workflow): the full
matrix above and CI on the exact final pushed commit, with terminal results inspected and confirmed failures fixed
without weakening checks. Other stages and documentation-only follow-ups run the affected local checks, keep required
automatic checks and record checks not run.

**No MKS SERVO42D/57D CAN fixture exists; never send family frames to the CL86-C bench (COM15).** While COMMON records
the owner as away, family hardware work does not start; a deferred-table row records it instead (contract 02 "Bench
steps and owner absence"; 12A derives the forbidden-command and allowed-write lists from the ledger). Before first power
establish board and revision, motor, wiring and termination (42D jumper R13, 57D SW1 pin 1), a free shaft and
authorization (12A step 1); first power assumes FFH or 9AH may move the shaft (12A step 2). Before any CAN step
establish from the menu the firmware (About), bitrate, slave ID, check mode, respond byte, work mode and 0_Mode (MK109
p.21-29); the group ID, the 8CH active byte and FFH's state, which have no menu item, by an established 00H read or as
recorded unknowns (the group ID and active byte as the declaration's explicit `UNKNOWN`, 02 step 1, with what each
admits in 02b step 5); a dedicated CAN interface with no CANopen node; and restart control. Host tests and fake drives
establish no hardware support.

## Handoff and progress

Each stage writes `docs/reports/mks_servo_can_<NN>.md` and updates `docs/prompts/mks_servo_can/PROGRESS.md` with:

1. Commits and push result; source and sibling revisions with adopted or rejected behavior (path and symbol).
2. Operations implemented versus gated, by ledger key, with MK-D dispositions and any new discrepancy.
3. Local checks and exact-commit CI, including checks not run or blocked.
4. Hardware NOT RUN, or exact fixture, firmware, edition and retained raw traffic.
5. Public API/CLI, coverage, backlog and roadmap changes; the DOCUMENTATION and MAINTAINABILITY records; seams
   claimed, landed or extended; next prerequisites.

Software states are NOT STARTED, IN PROGRESS, SOFTWARE COMPLETE, PARTIAL or BLOCKED (contract 02 "Documentation and
stage handoff"); SOFTWARE COMPLETE describes software only and hardware evidence is a separate column. A usable
artifact does not make its producer complete, and an inherited gate still blocks its dependent feature.
