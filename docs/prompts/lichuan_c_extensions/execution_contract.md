# Execution contract for every Lichuan C-family extension stage

Binding on every numbered stage in this directory. It adds to root [AGENTS.md](../../../AGENTS.md),
[COMMON.md](../COMMON.md) and the [shared integration contract](../canopen_shared/02_profile_integration_contract.md);
where those already say something, cite them instead of restating it.

## Read order

1. Root AGENTS.md and the parent Projects AGENTS.md (CI policy, ESP-IDF toolchains), then COMMON.md including
   the owner-absence section, then [README.md](README.md), this contract and the selected stage.
2. [CONVENTIONS](../CONVENTIONS.md) sections 1-4 and 6, [DOCUMENTATION](../DOCUMENTATION.md),
   [MAINTAINABILITY](../MAINTAINABILITY.md), [DELEGATION](../DELEGATION.md), the [MULTIDRIVE](../MULTIDRIVE.md)
   rows the stage touches, [ADD_DRIVE](../ADD_DRIVE.md) and the [multi-drive decision](../../multidrive_decision.md).
3. [canopen_shared README](../canopen_shared/README.md), [01](../canopen_shared/01_existing_code_and_tooling.md)
   and 02, the main [PROGRESS](../PROGRESS.md), this set's `PROGRESS.md` and the previous stage report.
4. Evidence: the [LX pack README](../../reference/lichuan_c_extensions/README.md),
   [review](../../reference/lichuan_c_extensions/review.md) and [SOURCES.json](../../reference/lichuan_c_extensions/SOURCES.json);
   the existing [Lichuan review](../../reference/lichuan_review.md), [ledger](../../reference/lichuan_objects.json),
   [operation map](../../reference/lichuan_operations.json) and their checkers; the CANopen principles pack
   ([principles.md](../../reference/canopen_principles/principles.md)) for CiA 301/402 facts and vendor differences.

## Repository and consumer boundaries

- Work on `main` under the current Git instruction (no new branch until version0). Only the lead commits and
  pushes; workers return disjoint changes. Preserve concurrent work, including other family promptsets that may
  land canopen_shared seams meanwhile. MotorControl-RS and FieldCore-node stay read-only; record the RS row
  required by AGENTS' recurring audit for each stage's seam.
- Profile directory and namespace are the existing ones: `include/MotorControlCAN/profiles/lichuan_c/`,
  `src/profiles/lichuan_c/`, `MotorControlCAN::LICHUAN_C`, `DriveProfile::LICHUAN_C`, inventory and JSONL
  `profileId` `lichuan_c`. No new `DriveProfile`, `Manufacturer`, profile directory, copied header or second
  ledger. A new header is allowed only for a distinct concern and is registered in every source, install and
  package list named in canopen_shared 02 (`library.json` and `check_repository.py` `EXPORTS` stay unchanged).
- The set name `lichuan_c_extensions` (this directory and the LX reference pack) names this directory's
  `PROGRESS.md` and the reports `docs/reports/lichuan_c_extensions_<NN>.md` (canopen_shared 02 "Names and
  layout"); the profile token stays `lichuan_c`.
- Installed core stays C++11, allocation-free, framework-independent and I/O-free, with caller-owned contexts,
  `prepare...`/`next...`/`advance...` phases, checked getters and atomic outputs (unchanged on failure).
- Appending enum values never renumbers an existing value. Existing CL86-C/OL86-C behavior, wire frames and
  tests stay unchanged unless a stage names the change and its migration entry.

## Evidence and supported behavior

- Original PDF pages and EDS lines are authoritative: LX pack `vendor/` for the extensions, `docs/reference/vendor/`
  for the C family. Extracts and the review are search aids; reopen the rendered page for every constant, bit,
  range, word, method or timing a stage turns into code. Cite as the review does (`TLCEN p11`, `TLEDS l.1569`).
- Keep each LX-D discrepancy open until cited new evidence resolves it; record the old claim, new evidence,
  model scope and disposition in the ledger, review notes, code, tests and Doxygen together.
- "Unchanged" in the delta review is a documentary claim for the new model, never evidence of shared firmware.
  No generic CiA 402 meaning, no C-family default and no CL86-C bench measurement becomes a TL-C or CL2 fact.
- A model without a ledger row for an object or operation refuses it before output change, admission or TX,
  with `UNSUPPORTED` and the operation's `CAPABILITY` detail. No success stub, placeholder row or raw-object
  escape hatch replaces typed coverage.
- Preserve uncertainty after every unconfirmed write; never replay. CAN TX or an SDO acknowledgement is not
  drive acceptance, motion or completion. Local cancel is not a stop.

## Identity and applicability

- No universal probe and no identity-based selection. `1000`, `1018`, `1009`, `100A` and `2000` are shared or
  undocumented across CL86-C, TL-C, DS-C and CL2 (backlog D18; LX-D01, LX-D02, LX-D20, LX-D35). The application
  declares the model per binding; identity reads only confirm or contradict it, with the outcomes and the
  vendor-object rule defined below. Stage 01 implements them for this profile.
- A model of another profile (DS-C, DS2-C) is never a LICHUAN_C model; it appears only as an identity
  reference that can contradict a declaration. No DS2 type enters this profile.
- Caller-supplied `Capabilities` remain the only source of motor kind, feedback, brake and calibration facts.
  TLO models and OL86-C have no encoder; a CL2 encoder resolution is a declared configuration fact.

### Identity outcomes (prompt 57's policy)

[Prompt 57](../57_regular_drive_operations.md)'s identity policy owns these outcomes: the declared binding
selects the profile and model, the identity read only confirms it, and the Lichuan family sets add
model-scoped checks on that one policy. The outcomes (CONFIRMED, UNCONFIRMED, CONTRADICTED) and the vendor-object rule are defined once, in prompt 57's
Identity policy bullet; this set and
[DS2](../lichuan_ds2_canopen/execution_contract.md#identity-and-applicability-backlog-d18) consume that
definition and do not restate it. The inputs below are what this set supplies to it.

- Inputs: the binding's declared model as a profile-qualified key (`DriveProfile` and the profile's model
  value), a current identity observation for the binding's node generation and server session, the
  application's candidate set for the interface, and identity references. A reference holds one model's
  identity fields and its basis: DOCUMENTED (a ledger claim with its page or EDS line) or MEASURED (an
  application-supplied specimen record with firmware provenance). Each set's model-scoped rule names the
  references that represent its models.
- Outcomes, their admission precedence ("Admission by outcome"), the vendor-object and motion rules and the
  stop exemption: prompt 57's Identity policy bullet.
- One comparison. It is pure, allocation-free and profile-neutral (no `LICHUAN_C` or `LICHUAN_DS2` type, so
  neither profile includes the other). The first of prompt 57, stage 01 here and DS2 stage 03 to land it
  implements it; the others extend it with their references and rules, and each records its location in its
  report. No stage writes a parallel check.

This set's model-scoped rule: references are MEASURED records only, because the documented `1000` and
`1018:01/02` of CL86-C, TL-C and DS-C are equal and CL2 documents no `1018`; the library embeds no record.
A TL-C declared while only an equal CL86-C record exists is therefore CONTRADICTED until a TL-C record exists
(07a T1). For the new models vendor-object downloads and every changing request follow prompt 57's
"Admission by outcome": only a current outcome in which a reference of the declared model matches admits,
stops exempt, so a TL-C or CL2 binding needs a MEASURED record of its own model first (07a T1, K2). This set
adds only the minimum record content (stage 01 item 6) and the model rows; protection beyond that rests on the
declaration.

### Routes by outcome (TL-C and CL2)

Prompt 57 keeps the bootstrap identity and state reads admitted under every outcome, refuses every changing
request without a declared-model match and admits one bootstrap NMT command. For a TL-C or CL2 binding the
routes that take no model follow this table at the example application's route check before owner admission
(03a items 2 and 6, consumed by 04a); preparations that take a model follow stage 01 item 7. Every row also
needs the binding's `NONE` attestation and S3's explicit no shape (03a item 2). The table is an identity
filter applied after model and object eligibility: a read is planned only for objects with a row for the
declared model ("Evidence and supported behavior"), so TL-C's raw bitrate candidates and error history are
refused before TX on a CL2 binding under every outcome, and CL2's caller-selected `1018` identity option
is its one undocumented read (04a items 1 and 2).

| Route | A declared-model match (CONFIRMED, or UNCONFIRMED with that match) | UNCONFIRMED without that match, or no current outcome | CONTRADICTED |
| --- | --- | --- | --- |
| Bootstrap reads: the `1000:00` probe, the identity read with its options (03a item 1, 04a item 2) and the state read (STATUS, MONITORS, FEEDBACK, ERRORS; CL2 without `1001`, 04a items 1 and 4) | Admitted | Admitted | Admitted (prompt 57) |
| Ordinary reads the declared model has rows for: configuration, I/O, tuning, raw filter candidates, TL-C bitrate candidates, `2203`, PDO map readback, TL-C error history | Admitted, explicit single reads | Admitted; the result carries the outcome | Refused before TX: only the bootstrap reads remain (prompt 57), and another product's read effects are unreviewed |
| Targeted NMT Enter Pre-operational to a node reported Stopped (prompt 57's bootstrap command) | Withheld (below) | Withheld | Refused (prompt 57) |
| Other targeted NMT commands (Start, Stop, Reset node, Reset communication) | Refused, NOT_DOCUMENTED | Refused (prompt 57) | Refused (prompt 57) |
| Node-0 NMT and SYNC on the interface | Refused by 03a item 6 until S13's full gate lands | Same | Same |

The bootstrap command is withheld because neither model documents NMT behavior or heartbeat production (LX
review "Interface and protocol": TL-C has only TLEDS `SimpleBootUpSlave=1` (l.27) and `1017` default 0
(l.236), no `100C` or `100D` and no manual text; CL2 documents none), and this set keeps heartbeat
observations documentary-unqualified (03a item 6) or raw (04a item 1), so no reviewed fresh Stopped report
exists for either model. A node that answers no SDO is reported stranded with the last state seen, never
Stopped by assumption; its recovery is the application's (an owner power cycle), with no other NMT command
and no broadcast as a fallback. A later reviewed change scoped to the model may admit the command on a
measured heartbeat record of that model. A read admitted before the outcome changes completes; its result
carries the outcome current at completion and admits nothing. A targeted NMT command admitted while the
node was declared or bound as another model is not covered by these refusals; 03a item 6's
population-change check refuses the declaration or rebinding until it is terminal or cancelled.

## Owner and bus

- One application receive owner per physical interface and one CANopen protocol domain; mixed models share it
  only at one bitrate with distinct node IDs, and node-number differences alone prove nothing more. A shared
  interface also needs the recorded analysis of canopen_shared 02 "Multi-profile consumers and package scope"
  (configured COB-IDs, NMT-broadcast and SYNC effects) and, until S13's full gate lands, 03a item 6's
  interface isolation (no SYNC and no node-0 NMT on an interface holding a TL-C or CL2 binding).
- No implicit enable, homing, fault clear, save, factory restore, NMT command, node or bitrate change, PDO
  reconfiguration or input reassignment on startup, selection, probe, identity read, recovery or cached read.
- TL-C: no path writes `2401..2407` or `2203`; a TL-C save is communication-affecting (TL57OP p5). CL2: no
  write to an object its manuals do not document, and no communication-record write at all.

## Verification

Inspect workflows and registrations first. Run focused tests, then the gates the change needs:

```sh
python scripts/check_promptset.py            # add --check-git-ref INDEX before, HEAD after, for reference/attribute changes
python scripts/check_lichuan_ledger.py
python scripts/generate_lichuan_objects.py --check
python scripts/check_lichuan_operations.py
python scripts/verify.py --mode quick
python scripts/verify.py --mode full
python scripts/check_firmware.py --target arduino   # and idf, idf-s2, s2, component per docs/verification.md
```

Firmware targets run from PowerShell with `--idf-path` as [verification.md](../../verification.md) and the
parent AGENTS describe. Model-scoped ledger checks are extensions of the three Lichuan scripts above, each with
negative cases in `scripts/check_lichuan_ledger_test.py` or `scripts/generate_lichuan_objects_test.py`.
A public API, generated-table, checker-registration or console change is a major change: verify required CI on
the exact final pushed commit. Ledger-only or documentation-only stages need local checks, not CI dispatch.

## Hardware

No TL-C or CL2 fixture exists, and no CL57-C/OL57-C fixture. Never send TL-C, CL2 or 57C-declared frames to the
CL86-C bench (COM15, nodes 1 and 2), and never declare a new model for a CL86-C node to exercise a refusal on
hardware; host tests do that. While COMMON records the owner as away, no family bench step runs. Before any
later bench step, establish board, transceiver, drive label, model, firmware evidence, wiring, node, bitrate,
DIP positions, the reviewed fixture electrical record of 07a (power connection, supply and connected I/O,
before first power) and the applicable authorization, and derive the
model's forbidden-object and allowed-RAM lists from its ledger rows. COMMON's owner-absence rule 2 lists the
CL86-C fixture's object numbers; it never stands in for a new model's list (on TL-C `2402` and `2407` are
bitrate candidates, LX-D03).

## Handoff and progress

Each stage writes `docs/reports/lichuan_c_extensions_<NN>.md` and updates this directory's `PROGRESS.md` with
the handoff items of canopen_shared 02: commits and push result; source and sibling revisions with adopted or
rejected behavior; operations implemented versus gated, by ledger key and model; LX-D and X-gate dispositions;
local checks and exact-commit CI, including those not run; hardware NOT RUN or exact fixture, firmware and raw
evidence; documentation and maintainability records; seams claimed, landed or extended; identity outcomes
and records used; next prerequisites. Software states are canopen_shared 02's NOT STARTED, IN PROGRESS,
SOFTWARE COMPLETE, PARTIAL and BLOCKED, with hardware evidence in its own column; SOFTWARE COMPLETE never
implies hardware qualification.
