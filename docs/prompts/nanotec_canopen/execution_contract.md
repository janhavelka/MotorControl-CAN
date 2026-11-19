# Execution contract for every Nanotec CANopen stage

These rules bind every stage of this set. Where they are stricter than a general
contract, they apply in addition to it; they never relax one.

## Read order

1. Root [AGENTS.md](../../../AGENTS.md), the parent Projects AGENTS and
   [COMMON.md](../COMMON.md), including its owner-absence section.
2. [CONVENTIONS.md](../CONVENTIONS.md) sections 1-4, 6 and 7, [DOCUMENTATION.md](../DOCUMENTATION.md),
   [MAINTAINABILITY.md](../MAINTAINABILITY.md), [DELEGATION.md](../DELEGATION.md), the
   [MULTIDRIVE.md](../MULTIDRIVE.md) rows the stage touches, [ADD_DRIVE.md](../ADD_DRIVE.md)
   and the [multi-drive decision](../../multidrive_decision.md).
3. [canopen_shared](../canopen_shared/README.md): its [01 inventory](../canopen_shared/01_existing_code_and_tooling.md)
   and the binding [02 integration contract](../canopen_shared/02_profile_integration_contract.md),
   with the seam table's current claims and the [audit application](../canopen_shared/audit_application.md)
   (answered and open questions). Cite them; do not restate or fork them.
4. The [pack README](../../reference/nanotec_canopen/README.md), [review](../../reference/nanotec_canopen/review.md)
   and [SOURCES.json](../../reference/nanotec_canopen/SOURCES.json); the CANopen
   principles pack (`docs/reference/canopen_principles/principles.md`).
5. This set's [README](README.md), `PROGRESS.md` (created by stage 01), the previous
   stage report and the selected stage.

## Repository and consumer boundaries

- Work on `main` under the current prompt42 instruction: no new branch until
  version0 is finished. Fetch and inspect branch, upstream, staged and unstaged
  changes before editing; fast-forward only when safe. Preserve concurrent work
  (other family sets are written in parallel) and stage only owned paths. The lead
  commits and pushes; workers return changes.
- Refresh MotorControl-RS HEAD and dirty state and read its profile seam
  (`docs/profile_contract.md`, `docs/prompts/lichuan_shared/`) under the AGENTS
  recurring audit; name each adopted or rejected behavior with path and symbol.
  Inspect FieldCore-node read-only for ownership changes. Edit neither.
- Names come from canopen_shared 02 "Names and layout". The set name
  `nanotec_canopen` names this directory, the reference pack, `PROGRESS.md` and
  `docs/reports/nanotec_canopen_<NN>.md`; the profile token `<family>`, proposed as the
  same `nanotec_canopen`, is recorded by stage 01. Code in `include/MotorControlCAN/profiles/nanotec_canopen/` and
  `src/profiles/nanotec_canopen/`, namespace `MotorControlCAN::NANOTEC_CANOPEN`,
  `DriveProfile::NANOTEC_CANOPEN`, `Manufacturer::NANOTEC`, inventory and JSONL
  `profileId` `nanotec_canopen`, console family `nanotec` in
  `examples/common/NanotecCanopenConsole.{h,cpp}`, fake server
  `test/support/NanotecCanopenDrive.h`. Stage 01 confirms these against the
  current checkout before the first landed name; afterwards a rename needs a
  migration entry.
- Reuse the single CANopen backend (`SdoClient` over CANopenNode), `CanBusOwner`,
  `Network.h`, `Pdo.h`, `Sync.h`, units, axis and operation vocabularies as listed in
  canopen_shared 01. Never reuse anything it lists as Lichuan-specific
  (`StatusLayout`, `StopKind`, `EnableGate`, 2300h save, DEC-1/2/8, Q/C identifiers).
  Change Lichuan code only through a shared seam with its own regression.
- Installed core: C++11, no heap, no I/O, clocks, sleeps, logging, threads, retries
  or persistence; free functions, fixed value types and caller-owned contexts with
  `prepare...`/`next...`/`advance...` and checked getters. Preparation and parsing
  publish atomically. Applications own the controller, time, queues and policy.

## Evidence and supported behavior

- Original pages are authoritative: N5 3.3.0 for the primary row, the C5-E 3.3.0,
  CL4-E 1.5.0 and PD4-C 3.3.0 manuals for their rows, and the stored all-objects EDS
  for each model and firmware. Reopen the cited PDF page (rendered where tables,
  bit diagrams or figures matter) and EDS section for every constant, bit, sequence
  or limit a stage turns into code. Text extracts and the review are intake aids.
- Discrepancies are NT-D01..NT-D31 (review) and NT-D32 (NanoJ autostart, recorded
  by stage 01 item 4). Close one only with cited new evidence, recorded
  in the object ledger `conflicts` with the original claims kept under their hash
  guard. A newly found contradiction gets the next number (NT-D33 onward) in the
  ledger and a dated note appended to the stage report; existing review text is
  never rewritten.
- The CANopen principles pack describes the standard and where vendors differ
  (CP-D01..CP-D28). A CiA 301 or 402 fact applies to Nanotec only where a Nanotec
  page states it. Manual arrows and command sequences are examples, not scripts.
- Unsupported, gated or unreviewed operations fail before admission and before TX,
  with a profile reason. No placeholder success, no raw-object escape hatch counted
  as coverage, no EDS default written at initialization, no RW flag treated as a
  write permission.
- Keep documented, implemented, host-tested, firmware-built and hardware-qualified
  coverage separate in the ledger, `docs/nanotec_canopen_coverage.md` and PROGRESS.

## Identity, applicability and control paths

- No universal identity probe. A `1000h` or `1018h` reply proves a responding CANopen
  server, not a Nanotec model or firmware. The application declares the model and
  binds the profile explicitly: `Endpoint.profile` holds the `DriveProfile` value and
  the S2 lookup resolves it (MULTIDRIVE row 57, backlog D18: no identity-based
  selection); the identity read only confirms and refuses mismatches before any
  vendor-object write. Each coverage row names the identity it consumes, its holder
  and invalidators (canopen_shared 02 "Identity and device evidence").
- An admitted row matches exactly: 1018h:01 = 0000026Ch, 1018h:02 in the declared
  model's table, the full 32-bit 1018h:03 and the 100Ah build string in the row, and
  1000h matching the model's motor class. The high word of 1018h:03 equalling the
  firmware number is an inference (review section 4); report it, never gate on it.
  FIR-v1650, FIR-v2538, CL4-E FIR-v2315 and unknown builds are refused for every
  dependent operation while their reads stay available.
- 1009h is "0" (NT-D29): hardware revision is a caller declaration from the
  nameplate. Motor kind, feedback and closed/open loop are caller-supplied
  `Capabilities`; the profile checks them against 1000h, 3202h and 3203h readback
  and refuses contradictions, but infers nothing.
- CiA 402 is not the only command path. The Plug & Drive interface (2290h, enabled
  ex works), a NanoJ program (2300h/2301h), N5 Ethernet REST, the C5-E/CL4-E USB
  cfg.txt and CL4-E Modbus RTU reach the same object dictionary. Stage 04b defines
  the exclusive-control admission every motion and settings stage applies.

## Owner and bus rules

- One application owner per physical interface receives and dispatches every frame.
  Nanotec uses 11-bit identifiers only (N5 p117) and shares the CANopen domain with
  Lichuan; node-ID uniqueness, bitrate and identifier plans are application facts
  checked at binding. Different node numbers do not prove a safe shared bus. By
  default a Nanotec axis runs on its own interface; sharing one with Lichuan waits
  for stage 11's recorded same-domain analysis (canopen_shared 02 "Multi-profile
  consumers and package scope").
- Per binding, a Nanotec route has SDO reuse policy `NONE` until its markers are
  reviewed (S10, 02b) and no best-effort stop until its reviewed shape is set (S3,
  04). Lichuan's DEC-1 markers and DEC-2 word never become Nanotec defaults.
- Console routes and API/CLI parity tests named in 02b-10 are Blocked until S4 lands
  (canopen_shared README, "Seams"; [open question](README.md#open-questions) 1). The
  stage delivers its API and owner tests, records the console part as Blocked, and is
  PARTIAL rather than SOFTWARE COMPLETE until that part lands.
- No implicit enable, homing, fault reset, save, restore, reboot, NMT command, PDO
  change, Plug & Drive change, NanoJ start or stop, or auto setup on startup,
  selection, probe, recovery or cached reads. Local cancel and recovery are not
  stops; NMT stop is not a quick stop; host heartbeat observation proves nothing
  about the drive's own reaction to host loss.
- Each operation declares its `RetryClass`. Enable, motion starts, homing, auto setup,
  save, restore, reboot and NMT are `MOTION_LIKE` and never repeated. An
  unconfirmed write keeps `UNKNOWN` execution; one immutable deadline per operation
  (`now >= deadlineUs` expires); a CAN ACK or TX report is not SDO success, and SDO
  success is not drive acceptance or motion completion.

## Verification

Inspect `.github/workflows/verify.yml`, [verification.md](../../verification.md) and
the current registrations first, then run focused tests and the gates the change
affects:

```sh
python scripts/check_promptset.py
python scripts/check_promptset.py --check-git-ref INDEX   # before committing reference, ledger or .gitattributes changes; HEAD after
git diff --check
python scripts/check_repository.py
python scripts/generate_lichuan_objects.py --check
python scripts/check_lichuan_operations.py
python scripts/verify.py --mode quick
python scripts/verify.py --mode full --compiler gcc --build-dir .build/nanotec-gcc
python scripts/verify.py --mode full --compiler clang --sanitize --build-dir .build/nanotec-asan
python scripts/check_firmware.py --target arduino
python scripts/check_firmware.py --target idf --idf-path <pinned ESP-IDF 5.5.5 per verification.md>
```

Stage 02 adds `python scripts/check_nanotec_canopen_operations.py` (with seam S7),
`python scripts/check_nanotec_canopen_ledger.py` and
`python scripts/generate_nanotec_canopen_objects.py --check`, each with a registered
negative test, and registers them in `scripts/verify.py`, `scripts/check_repository.py`
and CTest. Firmware targets run when firmware integration or application code
changes. CI follows impact (canopen_shared 02 status): registration (02b) and each
later major change (new profile API or behavior, owner or protocol integration,
persistence, build, toolchain or workflow) run the full matrix and verify the terminal
CI result of the exact final pushed commit; other changes run the affected local
checks, keep the required automatic checks and record checks not run.
Documentation-only stages and follow-ups need local checks only. Never cite an older green run.

## Hardware

No Nanotec fixture exists. Never send Nanotec frames to the CL86-C bench (COM15,
S2 board, nodes 1 and 2) and never open COM15 from family work. While COMMON records
the owner as away, no family bench step runs; deferred steps go to the
[deferred table](../README.md#deferred-to-owner-return) only once a Nanotec fixture
is part of the recorded setup. The standing bench authorization and the absence
waiver establish no Nanotec fixture. Before any later bench step, record board,
transceiver, model, firmware build, motor and feedback configuration, wiring,
switches, node ID, bitrate and the user's authorization, and derive the forbidden
and RAM-only object lists from the ledger. A host test or fake-server run is never
hardware evidence.

COMMON owner-absence rule 2 lists the CL86-C fixture's object numbers; they do not
transfer by number (Nanotec 2300h is NanoJ control, not save; MULTIDRIVE case 7).
This set's own persistent or communication-changing operations are stage 10's saves,
restores, restart, node, bitrate, 2007h:01 and 1F80h setters, stage 09's COB-ID and
mapping writes, and 04b's auto setup (which saves 1010h:05/06) and NanoJ start.

## Handoff and progress

Each stage writes `docs/reports/nanotec_canopen_<NN>.md` and updates `PROGRESS.md`:

1. Commits and push result, verified with a fresh fetch and ahead/behind check.
2. Source and sibling revisions (RS HEAD and seam, FieldCore-node) with adopted or
   rejected behavior.
3. Operations implemented versus gated, by ledger key, with NT-D dispositions.
4. Local checks and exact-commit CI, including blocked and not-run checks.
5. Hardware NOT RUN, or the exact fixture, firmware build, raw traffic and restoration.
6. The DOCUMENTATION and MAINTAINABILITY records; seams claimed (before the shared
   edit), landed or extended, with the canopen_shared README claim and landed columns
   (commit, tested interface, consumer gate) updated.
7. Public API, CLI, coverage, backlog and roadmap changes; next prerequisites.

Software status per stage or block uses canopen_shared 02's states: NOT STARTED,
IN PROGRESS, SOFTWARE COMPLETE, PARTIAL or BLOCKED. Hardware evidence has its own
column and never changes the software status. SOFTWARE COMPLETE means every owned
deliverable and check closed, with named gates recorded as inherited; a stage with a
Blocked deliverable is PARTIAL.
