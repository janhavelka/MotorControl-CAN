# Execution contract for every Lichuan DS2-C stage

## Read first, in this order

1. Root [AGENTS.md](../../../AGENTS.md), the parent Projects AGENTS (CI policy, ESP-IDF toolchains) and
   [COMMON.md](../COMMON.md), including the owner-absence section.
2. [CONVENTIONS.md](../CONVENTIONS.md) sections 1-4, 6 and 7, [DOCUMENTATION.md](../DOCUMENTATION.md),
   [MAINTAINABILITY.md](../MAINTAINABILITY.md), [DELEGATION.md](../DELEGATION.md), the
   [MULTIDRIVE.md](../MULTIDRIVE.md) rows the stage names, [ADD_DRIVE.md](../ADD_DRIVE.md) and the
   [multi-drive decision](../../multidrive_decision.md).
3. [canopen_shared](../canopen_shared/README.md), its [inventory](../canopen_shared/01_existing_code_and_tooling.md)
   and the binding [integration contract](../canopen_shared/02_profile_integration_contract.md). Cite them; do not
   restate them in family code or documents.
4. The [promptset index](README.md), this contract, the selected stage and this directory's `PROGRESS.md`.
5. The [pack](../../reference/lichuan_ds2_canopen/README.md), [review](../../reference/lichuan_ds2_canopen/review.md),
   [SOURCES.json](../../reference/lichuan_ds2_canopen/SOURCES.json) and the original page or EDS line behind every
   claim the stage uses; the [CANopen principles pack](../../reference/canopen_principles/principles.md) for CiA 301/402
   mechanics and where vendors differ.
6. For the Lichuan C boundary only: [lichuan_review.md](../../reference/lichuan_review.md) and the `LICHUAN_C` headers.

Then run the recurring source audit of root AGENTS: MotorControl-RS HEAD, dirty state and its profile seam
(`docs/prompts/lichuan_shared/`, `docs/profile_contract.md`), FieldCore-node read-only. Write "unchanged since stage
NN" when HEAD, dirty state and seam match the last recorded row. Original rendered pages and EDS lines are
authoritative; extracts are for search. Do not substitute a CL86-C, TL-C, DS-C or another vendor's manual.

## Repository and consumer boundaries

- Git follows COMMON: work on `main`, no new branch, the lead alone commits and pushes; subagents get disjoint paths
  or read-only reviews under DELEGATION. Preserve concurrent work and stage explicit owned paths.
- Sibling repositories stay read-only. FieldCore-node is inspected, never edited or claimed as integrated.
- Names follow canopen_shared 02 "Names and layout": token `lichuan_ds2`; `include/MotorControlCAN/profiles/lichuan_ds2/`,
  `src/profiles/lichuan_ds2/`, namespace `MotorControlCAN::LICHUAN_DS2`, `DriveProfile::LICHUAN_DS2`; the existing
  `Manufacturer::LICHUAN` is reused. JSONL `profile` is `lichuan_ds2`; the console family is `ds2` in
  `examples/common/LichuanDs2Console.{h,cpp}`; the fake server is `test/support/LichuanDs2Drive.h`. Ledgers
  `docs/reference/lichuan_ds2_objects.json` and `lichuan_ds2_operations.json`; scripts
  `check_lichuan_ds2_ledger.py`, `generate_lichuan_ds2_objects.py`, `check_lichuan_ds2_operations.py`; coverage
  `docs/lichuan_ds2_coverage.md`. The set name `lichuan_ds2_canopen` (this directory and the reference pack)
  names this directory's `PROGRESS.md` and the reports `docs/reports/lichuan_ds2_canopen_<NN>.md`; the profile
  token `lichuan_ds2` names everything else above. Stage 01 confirms no name collides at HEAD.
- Console routes wait for seam S4. Every console SDO read goes through the Lichuan-typed `ProbeApp`
  ([canopen_shared README](../canopen_shared/README.md)), so each `ds2` console route, its API/CLI parity and
  common-verb dispatch stay BLOCKED until prompt 64 lands, unless the prompt 64 lead or the user decides that a
  minimal family read may extend `ProbeApp` first (open question 2 of the shared
  [audit application](../canopen_shared/audit_application.md)). A stage then delivers its API, owner and fake-server
  tests and keeps its console items BLOCKED in `PROGRESS.md`.
- The installed core stays C++11, deterministic and bounded: free functions, fixed value types, caller-owned contexts,
  `prepare...`/`next...`/`advance...`, checked getters, atomic publication with documented count-reset exceptions, no
  heap, I/O, clock, sleep, logging, retry or replay. Applications own the controller, time, queues and policy.
- One profile serves nine models. A profile-local declared `Model`, with the declared encoder suffix, selects each
  documented model fact (status dialect, speed-limit object, node range, brake form, EDS scope). Never split public
  APIs by motor technology and never select a model from a reply.

## The Lichuan C boundary

| Item | Decision for DS2 | Reason |
| --- | --- | --- |
| `Manufacturer::LICHUAN`, CANopen services, owner, `SdoClient`, codecs | Reuse as is | canopen_shared 01 "Reuse as is" |
| Ledger, generator, checker and packaging conventions | Copy the convention, not the content | canopen_shared 01 tooling table |
| CDN retrieval with the linking page as Referer | Reuse the procedure for refreshed downloads | Pack SOURCES.json limitations |
| Vendor document habits: printed versus PDF page offsets, "about 10 ms" enable example, arrow tables 15 to 31/95/63/127 and 15 to 11, bits 0..2 for controlword 0/6/7/15, template EDS defaults, vendor 0x1000 | Reviewer alert only | Recurring habits are not firmware evidence; the DS2C status table equals the EN86 table text and is still another product |
| `StatusLayout`, `interpretStatusword`, `STATUS_EN86_ZERO_SPEED`, `MONITOR_*` (`2100`) | Must not reuse | DS2 has its own two dialects (DS-D04) and no `2100` |
| Lichuan C's definitions of objects `2100`-`2102`, `2200`-`2203`, `2300`-`2302`, `2400`-`2407`, `2500`-`2520` | Must not enter DS2: no DS2 ledger row, symbol or access uses them | DS2 EDS files declare no manufacturer objects. IBL `2406h` is DS2's own manual-only row (02 item 5) with another meaning, a torque speed limit where CL86-C has a DI filter; its write stays withheld (06b) |
| `StopKind` (010F, 000B), `stopPredecessorAllowed`, DEC-2, DEC-8 | Must not reuse | DS2 documents only 15 to 11 in position mode; DS2 records its own `DS-DEC-n` |
| DEC-1 read acceptance, DEC-3 drain premise, `SdoReuse` default markers | Must not inherit | DS2 needs its own read-effect decisions: `DS-DEC-1` (03) before polling, a separate one before markers (S10) |
| `AlarmClearMethod`, `prepareSetDevicePosition`, `prepareSave`/`prepareFactoryRestore` (`2300`) | Must not reuse | No DS2 object (DS-D20, DS-D21) |
| `IdentityWidths`, `CountWidths::MEASURED_U16`, `EnableGate`, `ENABLE_SERVICE_STEPS`, S20/S21, `2201` pulse limit, `MotionIntent` | Must not reuse | CL86-C specimen and bench facts |
| Q01-Q28, conflict IDs, scope `eds_family_unbound`, console verbs of `lichuan_c` | Must not reuse | DS2 uses DS-Dnn, `DS-Gnn` and its own `ds2` grammar |
| Bits 0..2 projection for controlword 0/6/7/15 | Seam S8 candidate only | Documented identically (EN86 p12, DS2C p12); extract only an identical helper |
| The identity comparison of prompt 57's policy (outcomes, vendor-object rule, motion rule) | Extend the one profile-neutral comparison | Defined once in prompt 57's Identity policy bullet; the comparison carries no `LICHUAN_C` type |

## Evidence and supported behavior

- Record per object: index, sub-index, type, access, units, PDO flag, model scope, page or EDS line, every conflicting
  claim. Conflict IDs are DS-D01..DS-D32 from the pack review and DS-D33, DS-D34 from stage 01's intake
  corrections; gates are `DS-Gnn`; decisions are `DS-DEC-n` with `selected_by` and
  `selected_on`. Close a DS-D only with a newer original document, vendor clarification or fixture evidence scoped to
  model and firmware, and record the old claim beside the replacement.
- No generic CiA 402 assumption. The principles pack corroborates mechanics; DS2 behavior comes only from DS2 sources.
  Undocumented: controlword fault reset, halt bit 8, `605A`, `6007`, `1016`, `1029`, `6502`, `1010`/`1011`, node
  guarding, statusword bits 4, 5, 6, 9, 11, 13, 14 and the fault and switch-on-disabled states.
- Unsupported operations return `UNSUPPORTED` with the operation's `CAPABILITY` detail before admission and TX, and the
  coverage file states the evidence reason. No success stub; a raw object accessor does not count as coverage.
- Unresolved interpretations (ramp and velocity units, signed encodings, relative basis, completion bits) are never
  defaulted. Following the 2026-11-17 direction (settable guards, user responsibility), a stage may offer an explicit
  caller declaration, bound to binding and configuration generations and copied into every terminal result. Without
  it the dependent common guarantee rejects before TX; a native acknowledged-only path may remain. A declaration is
  caller evidence, not a resolved DS-D.
- Enabling in modes 3 and 4 starts motion with the retained targets (DS2C p12-13): there it is the start step of 06 or
  06b, never a standalone action, and a mode change while enabled is refused.
- Persistence of SDO writes is undocumented (DS-D20; `P02-31`, DS2C p16; `P0A-03`, DS2C p21; "RW/S", DS2C p24-26).
  Every settings write reads the prior value first and leaves a restore record; no write is called volatile before a
  restart test.
- Never read or write an object absent from the declared model's sources, scan `2000h`-`5FFFh`, or write a vendor
  object outside the identity rules below.

## Identity and applicability (backlog D18)

There is no universal probe: a `1000` or `1018` reply proves a responding CANopen server only. Binding is the
application's declared model, encoder suffix, node and bitrate; selection performs no I/O. Identity reads confirm or
contradict, never select. [Prompt 57](../57_regular_drive_operations.md)'s Identity policy bullet defines the outcomes
(CONFIRMED, UNCONFIRMED, CONTRADICTED), their inputs, the one comparison, the vendor-object rule and the motion rule.
DS2 consumes that definition and supplies only its references, candidates and the admission table below; it
restates no outcome and writes no parallel check.

DS2's references are the DOCUMENTED claim sets per model, each holding the product fields `1000`, `1018:01` and
`1018:02`. `1000` is 0x00040192 in every source (DS2C p24, every EDS). The `1018:01`/`:02` pairs are 0x0100/0x0002
in every family manual (DS-D01), 0x1000/0x2000 in DS2EDS (DS2 group) and ISLEDS (ISL models) and 0x1000/0x3000 in
IBLEDS (IBL models); DS2-Mini has the manual claim alone. MEASURED references (an application-supplied specimen
record, prompt 57's inputs; 12a adds the first) join them. Every DS2 model and the D18 reference 0x1000/0x0001
(CL86-C, TL-C and DS-C EDS, and the measured CL86-C identity recorded under Q01, used only as a foreign reference)
are always candidates, whatever the application lists. `1000` reads as a stepper under the N5 description
(DS-D03), so it never yields a motor kind.

Consequences of these references, which 03 tests: no DOCUMENTED pair can give CONFIRMED, because the manual pair
matches all nine models, the DS2EDS/ISLEDS pair five and the IBLEDS pair two. A live pair that only other models'
references hold (the IBLEDS pair on a DS2 or ISL declaration, the DS2EDS/ISLEDS pair on a DS2-Mini or IBL
declaration) is CONTRADICTED until a MEASURED reference for the declared model exists. A live `1000` other than
0x00040192 matches no reference. No caller acceptance replaces a reference (prompt 57): when no reference of the
declared model matches, the application supplies a MEASURED reference for its specimen, with firmware provenance.

| Current outcome (prompt 57) and DS2 case | Bootstrap reads (probe, identity, state) | Configuration reads (03 groups) | Changing requests (mode select, enable, release, motion, settings, `1017` write, targeted NMT) | Explicitly requested stop (04's `QUICK_STOP`) | Vendor object `2406h` (IBL models only) |
| --- | --- | --- | --- | --- | --- |
| No current outcome: never read, or cleared by canopen_shared 02's invalidators | Allowed | Refused until a fresh identity read | Refused until a fresh identity read | Admitted | Refused |
| CONFIRMED, or UNCONFIRMED with a declared-model match | Allowed | Allowed | Allowed; results carry every matching model and its basis ("documented, not observed" for DOCUMENTED); every interpretation uses the declared model's tables only (03 item 4) | Admitted | Read allowed; the write stays withheld (06b) |
| UNCONFIRMED without a declared-model match, every product field read (a differing `1000` included) | Allowed | Allowed | Refused; the application supplies a MEASURED reference or corrects the declaration | Admitted | Refused |
| UNCONFIRMED with a product field unread (abort, timeout or not announced by `1018:00`) | Allowed | Allowed | Refused; read the field again | Admitted | Refused |
| CONTRADICTED | Allowed | Refused | Refused before TX by prompt 57's motion rule; nothing overrides it | Admitted under prompt 57's stop exemption; the result carries the contradicted identity and claims no standstill | Refused (prompt 57's vendor-object rule for downloads; DS2 refuses the upload too) |

An explicitly requested stop still passes its own route, binding-generation and source-state checks (04; prompt 57's
exemption removes only the identity refusal); refusing it on identity grounds could leave a moving drive running. An
NMT command is never that stop. Firmware identity (`1009`, `100A`, `1018:03`) is reported and never refuses (prompt
57). Every stage with a changing request (04 to 10) tests a correctly labelled DS2 binding on a foreign fake server
(0x1000/0x0001): zero changing TX, while 04's `QUICK_STOP` stays admitted with the contradicted identity in its result.

A collision that includes the declared model admits by the declaration (prompt 57 "Admission by outcome"; lead
decision of 2026-11-19, [audit application](audit_application.md) round 2). The colliding siblings can differ: IBL60
and IBL80 share the IBLEDS pair but use different status dialects, and the DS2EDS/ISLEDS pair spans both dialects
(review "Status-word dialects"). The application owns the declaration; the result names every matching model and
nothing is decoded with a sibling's tables. 03 tests sibling servers under each colliding pair.

Targeted NMT commands (09) are changing requests in the table above and never the stop exemption. DS2 documents no
NMT effect on the drive (DS-D32), so each also needs the condition below before TX; a command without a row stays
refused.

| NMT command to a DS2 node | Condition before TX, beyond the identity table | Effect to record |
| --- | --- | --- |
| Start Remote Node (01h) | A fresh report that 04's `released` predicate accepts and no pending or non-terminal operation on the binding | PDOs flow in Operational (CiA 301, principles pack section 3); drive reaction undocumented; the owner sends no RPDO (09 item 4) |
| Stop Remote Node (02h) | Same | A Stopped node serves only NMT and error control (principles pack section 3), so 04's SDO stop route is lost until a Start |
| Enter Pre-operational (80h) | Same | PDOs stop; drive reaction undocumented |
| Reset Node (81h), Reset Communication (82h) | Same | Reset and the following boot-up invalidate dependent evidence and queued work (canopen_shared 02, 10 item 7) |
| Any command to node 0 | Refused by S13's default deny on every interface with a DS2 binding (owner bullet below) | No DS2-local guard |

## Owner and bus rules

- One `CanBusOwner` per interface receives every frame; DS2 uses the existing services and never forks them.
- DS2 shares an interface with Lichuan C nodes only as one CANopen domain: one bitrate, distinct node IDs, COB-IDs
  read live and without overlap (DS-D06, DS-D07), reservations by bus and address whatever the profile label, and
  only once canopen_shared seam S13 has landed: it admits SYNC and every NMT command to node 0 on a mixed interface
  only for reviewed effects on every declared node, so DS2's unknown effects refuse both (11). Until then each
  family has its own interface. S13's default deny lands earlier, in the S2 commit (02b): from DS2's first binding
  prompt 51's producer refuses SYNC and the owner refuses node-0 NMT on every interface with a DS2 binding, a
  DS2-only one included, and a schedule running when a DS2 binding is admitted is revoked first (09).
- No implicit enable, mode change, homing, fault clear, NMT command, PDO or heartbeat change or parameter write on
  startup, selection, probe, recovery or cached reads. Never enable to prepare a stop. Release, stop, NMT stop, cancel
  and recovery stay distinct. Write preflight, the one operation deadline and the source-state table follow
  canopen_shared 02 "Operation admission"; no automatic repeat; an unconfirmed write keeps `UNKNOWN` execution and
  is never replayed. CAN TX or ACK is not SDO success; SDO success is not drive acceptance.
- The drive documents no reaction to host loss (DS-D32). Host durations are host intent; a wired stop is the
  application's ([prompt 62](../62_stop_line.md)).

## Verification

```sh
python scripts/check_promptset.py            # every stage; --check-git-ref INDEX/HEAD for reference artifacts
git diff --check
python scripts/verify.py --mode quick
python scripts/verify.py --mode full         # includes Doxygen; use for API, build or registration changes
python scripts/check_firmware.py --target arduino
python scripts/check_firmware.py --target idf --idf-path <pinned ESP-IDF 5.5.5>
python scripts/check_firmware.py --target component --idf-path <pinned ESP-IDF 5.5.5>
python scripts/check_lichuan_ledger.py && python scripts/check_lichuan_operations.py
python scripts/check_lichuan_ds2_ledger.py && python scripts/generate_lichuan_ds2_objects.py --check
python scripts/check_lichuan_ds2_operations.py   # the three DS2 checks exist from stage 02
```

Use the environment in [verification.md](../../verification.md) (PowerShell, pinned toolchains; never a global IDF).
A new profile API, registration, build or cross-module change is major: verify required CI on the exact final pushed
commit and report checks not run. Documentation-only edits need local checks only.

## Hardware

No DS2-C, DS2-Mini, IBL-C or ISL-C fixture exists. Never send family frames to the CL86-C bench (COM15, nodes 1 and
2) and never open COM15 from family work. The owner-absence rules and waiver establish no family fixture. Before a
later bench step establish board, transceiver, exact model and suffix, firmware identity, motor, brake, wiring and
DI assignments, DIP node, bitrate, termination, supply, shaft condition and applicable user authorization, and derive
forbidden and allowed object lists from the DS2 ledger. COMMON's owner-absence rule 2 lists the CL86-C fixture's
object numbers and never stands in for that list.

## Handoff and progress

Each stage writes `docs/reports/lichuan_ds2_canopen_<NN>.md` and updates `PROGRESS.md` with the canopen_shared 02
handoff items, plus: DS-D dispositions touched, identity outcomes and declarations used, Lichuan C items reused or
rejected, console items BLOCKED on S4, and next prerequisites. Software states are canopen_shared 02's: NOT STARTED,
IN PROGRESS, SOFTWARE COMPLETE, PARTIAL or BLOCKED. Hardware evidence has its own column (NOT TESTED, or exact
fixture, firmware, image and raw-evidence links). Inherited gates are listed and never change a stage's state.
