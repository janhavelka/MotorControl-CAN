# 02 — Profile integration contract

Execute this stage under the family's execution contract.
Status: NOT STARTED. Binding on every family stage from the one that registers the profile,
after the family's ledger and codec checks pass. The first family lands each shared seam
once; later families extend it ([README](README.md) table). Read the family's
[stage 01](01_existing_code_and_tooling.md) record, the [CLI contract](../../cli_contract.md),
[owner guide](../../can_owner.md) and [communication practice](../../communication_practice.md).
Commit tooling, core/profile and application/console changes separately, each tested.

## Work

### Names and layout

- `<family>` is lower snake case `<manufacturer>_<family>`, as `lichuan_c`. It names
  `include/MotorControlCAN/profiles/<family>/`, `src/profiles/<family>/`, the inventory
  `profileId`, the JSONL `profile` value and the family's files. Its upper case names
  `MotorControlCAN::<FAMILY>` and `DriveProfile::<FAMILY>`; `Manufacturer::<MANUFACTURER>`
  is added once per manufacturer. A landed name changes only with a migration entry.
- Common operations keep the MotorControl-RS names and phases of CONVENTIONS section 3
  where the meaning matches (`prepareIdentity`, `prepareEnable`, `prepareStop`,
  `prepareMoveAbsolute`, `prepareVelocity`, `prepareHome`, `prepareSave`, with their
  `next...`/`advance...` calls and checked getters). Contexts are concrete per family;
  no `getResult`, `advanceOperation` or universal context.
- Native CANopen names stay native (`makeNmtRequest`, `checkNetworkEvent`,
  `validatePdoMap`, `prepareSync`, statusword and controlword fields); a vendor feature
  without a common meaning takes its manual's name. Equal words or object numbers never
  justify reusing a Lichuan type such as `StopKind` or `StatusLayout`.

### Object and operation ledger

- `docs/reference/<family>_objects.json`, schema 2, with the top-level keys and entry
  fields of `lichuan_objects.json`: per entry `key`, `scope_id`, `type`, EDS and reviewed
  access and PDO mapping, `units`, `scaling`, `effect`, `prerequisites`, `source_refs`
  (PDF page or EDS line), `conflict_ids`, `coverage` (independent `documented`,
  `implemented`, `host_tested`, `hardware_qualified`) and `review` (`read_effect`,
  `write_policy`, `gate_ids`). Prose-only functions go in `manual_only`; `conflicts` keep
  every original claim under a hash guard; `owner_prompt` names the family's stage IDs.
  No EDS default is written at initialization and an RW flag grants no write.
- `docs/reference/<family>_operations.json`, schema 2, with Lichuan's keys (`families`
  with `header`, `functions`, `tests`, `console`; `ledger_bindings`; `python_plans`). It
  references the ledger and never copies device facts.
- `scripts/check_<family>_ledger.py`, `scripts/generate_<family>_objects.py` (`--check`
  never writes; outputs `Types.h`, `Objects.h`, optional `Catalogue.h`, `Pdo.h` where
  applicable, `docs/reference/<family>_generated.md`) and
  `scripts/check_<family>_operations.py`, each with a registered negative test: missing
  row, tampered hash, altered output, unclassified function, promoted gate. Add LF rules
  for generated headers to `.gitattributes`; extend `TYPE_CODES` only from the family EDS.
- Seam S7, once: scope `check_lichuan_operations.py` to common and `lichuan_c` headers
  with a regression that an unclassified common or Lichuan function still fails; the
  checkers together claim every public header exactly once. Register each in
  `scripts/verify.py`, `scripts/check_repository.py` (`CHECKS`, file lists) and CTest.

### Sources, hashes and rights

- Pack in `docs/reference/<family>/`: `SOURCES.json` shaped like
  [multidrive/SOURCES.json](../../reference/multidrive/SOURCES.json) (per source `url`,
  `retrieved_on`, `bytes`, `sha256`, `applicability`, `firmware_match`, `license`,
  `revision`, `pdf_pages`; `remote_only` entries with no path, hash or bytes), unmodified
  originals in `vendor/`, extracts in `extracted/` with one-based PDF pages.
- Add the manifest to `reference_manifests` in `scripts/check_promptset.py`; run it with
  `--check-git-ref INDEX` before the commit and `HEAD` after. Never run vendor tools.
- Vendor material is outside the MIT license and never enters `library.json` `export`,
  an install rule or a packed archive; the exact archive set in `test/package_test.py`
  proves its absence.

### Identity and applicability

- Seam S1: when the family's code lands, append it to `Manufacturer`, `DriveProfile` and a
  finite table in `src/Discovery.cpp` replacing `DiscoveryProfile`'s Lichuan defaults.
  `DiscoveryCapabilities.nonChanging`, `exactModel` and `firmware` stay false without
  family evidence; address bounds, probe object and model/firmware `scopes` come from the
  ledger. Unknown firmware keeps dependent operations gated even when reads succeed.
- No universal identity probe. A `1000` or `1018` reply proves a responding CANopen
  server, not the family, model or firmware. Selection performs no I/O; identity is an
  explicit read; a mismatch refuses dependent work.
- Seam S2: an explicit relation from `Endpoint.profile` to `DriveProfile`. Every family
  preparation, and every Lichuan one, refuses a binding of another profile before
  touching output or yielding work; test both directions and that nothing is sent.

### Capabilities and the common operation mapping

- The caller supplies `Capabilities` per configuration with evidence; the profile infers
  no motor kind, feedback, brake or calibration. An unsupported request returns
  `UNSUPPORTED` with the operation's `CAPABILITY` detail before admission.
- `docs/<family>_coverage.md` has one row per common operation (reads, enable, release,
  mode select, absolute, relative and angle move, finite velocity, homing, each stop kind,
  fault clear, save, restore): native objects and words by ledger key, staging and trigger
  order, acknowledgement and completion evidence, stop interaction, `RetryClass`, gates
  and tests, or the evidence-based reason it is unsupported.
- CiA 402 objects and words apply only where the family's own pages say so; manual arrows
  are examples. Never enable to prepare a stop or reset. Release, stop, NMT stop, cancel
  and recovery stay distinct. One immutable deadline per operation (`now >= deadlineUs`
  expires); no automatic repeat; an unconfirmed write keeps `UNKNOWN` execution. Units go
  through `prepareVelocityTarget` and `Native.h`; a missing rate basis is a reviewed core
  change, not a profile shortcut.
- Seam S3 (CANopen): the best-effort stop on an unresolved route becomes a per-profile
  reviewed shape instead of the hard-coded Lichuan `6040:00=000B`. Seam S8: extract a
  CiA 402 helper only when both callers need identical, evidenced semantics. Review the
  family's `SdoReuse` marker objects and their read effects before `setSdoReuse`. After
  prompt 64, implement the family's row of its axis table and pass that prompt's session
  and owner tests; add no second session.

### Console, help and parity

- One `commands[]` entry per family in `examples/common/ProbeConsole.cpp`, parser and
  formatter in `examples/common/<Family>Console.{h,cpp}`. Existing top-level verbs keep
  their Lichuan meaning; no global selected-profile mode. After prompt 64, common verbs
  may dispatch by the target axis's bound profile with unchanged grammar and units.
- Seam S6: the JSONL `profile` field names the admitted operation's `profileId`
  (`ProbeConsoleText.h` envelope, `PROFILE` in `scripts/console_jsonl.py`). `profile list`,
  `caps` and `help` derive from the same inventory and claim no detection. `nmt`,
  `heartbeat`, `network` and `sync` reach a family binding only when its evidence admits
  them, with unchanged meaning.
- Each family route calls its public API. Tests drive the API and the console through the
  real `CanBusOwner` and `SdoClient` and a fake server on the owner `Transport` callbacks
  (`test/support/<Family>Drive.h`, documented frames only, labelled synthetic) and compare
  TX frames and terminals. Cover foreign-profile refusal without TX, abort versus
  malformed reply, timeout and late identical reply, stale generation, retained results,
  cancel that is not a stop, urgent stop under queue pressure and no replay after a lost
  reply. The map's `console` rows make the operation checker prove each route.

### Multi-profile consumers and package scope

- One fake interface carries a Lichuan axis and a family axis under one owner: independent
  stops, reservations that conflict on the same bus and address whatever the profile
  label, separate caches and results. `test/multidrive_contract_test.cpp` still passes.
- Add sources to `MOTORCONTROLCAN_SOURCES` (the IDF component reuses it), an
  `install(FILES ...)` rule for `profiles/<family>/`, headers and sources to
  `test/package_test.py` (`HEADERS` and the exact archive set) and the export list in
  `scripts/check_repository.py`, and C++11 consumers of core only, Lichuan only, the
  family only and both. No
  profile-selection build option; catalogue strings stay optional. After prompt 65,
  follow its component layout.

### Documentation and stage handoff

- With the code: README scope, CHANGELOG `Unreleased` (landed code only), the coverage
  file linked from `docs/coverage.md` and `docs/profile_contract.md`,
  `docs/api_cli_mapping.md`, `docs/cli_contract.md`, `docs/source_map.md`, backlog,
  roadmap and strict-gate Doxygen for every public declaration. Documented, implemented,
  host-tested, firmware-built and hardware-qualified stay separate.
- Each stage writes `docs/reports/<family>_<NN>.md` and updates
  `docs/prompts/<family>/PROGRESS.md` (NOT STARTED, IN PROGRESS, PARTIAL, BLOCKED or
  COMPLETE; hardware in its own column). The handoff gives commits and push result; source
  and sibling revisions with adopted or rejected behavior; operations implemented versus
  gated, by ledger key; local checks and exact-commit CI, including those not run;
  hardware NOT RUN or exact fixture, firmware and raw evidence; the DOCUMENTATION and
  MAINTAINABILITY records; seams landed or extended; next prerequisites.

### Bench steps and owner absence

While [COMMON.md](../COMMON.md) records the owner as away, its rules bind every family bench
step: no waiting (a row in the [deferred table](../README.md) instead), nothing persistent,
no communication change, snapshot and restore, one lead on the port. Nobody can connect a
family drive then and the recorded fixture holds none, so family hardware work waits for the
recorded return; the waiver establishes no family fixture. Family work never sends frames
to the CL86-C nodes or opens COM15. Before a later bench step, derive the family's
forbidden-object and allowed-RAM lists from its ledger and establish board, transceiver,
drive, firmware, wiring, node, bitrate and authorization.

## Deliver

- Seams S1, S2, S6, S7 and, for a CANopen family, S3 landed or extended, with the README
  table updated; S4, S5 and S9 consumed only from their prompts.
- Inventory entry and guards; a real probe (reviewed or explicitly accepted read effect,
  never a write) through API, application and console; registered ledger checkers; parity
  and multi-profile tests; package entries; documentation; report; PROGRESS row.

## Acceptance

The probe and each delivered route run through the real owner, `SdoClient` and fake server;
foreign-profile refusals send nothing in both directions; JSONL records carry the admitted
profile; checker negative tests fail as intended; all consumers build in C++11.
`python scripts/verify.py --mode quick` and `--mode full`, the firmware builds in
[verification.md](../../verification.md) and the family checks pass, and CI passes on the
exact final pushed commit. Host tests establish no hardware qualification.
