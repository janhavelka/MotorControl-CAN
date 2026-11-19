# Execution contract for every MOONS' CANopen stage

Binding on every stage in this directory. It applies root [AGENTS.md](../../../AGENTS.md), the parent Projects
AGENTS, [COMMON.md](../COMMON.md) (including the owner-absence section), [CONVENTIONS.md](../CONVENTIONS.md),
[DOCUMENTATION.md](../DOCUMENTATION.md), [MAINTAINABILITY.md](../MAINTAINABILITY.md),
[DELEGATION.md](../DELEGATION.md), [MULTIDRIVE.md](../MULTIDRIVE.md), [ADD_DRIVE.md](../ADD_DRIVE.md) and the
[multi-drive decision](../../multidrive_decision.md). Where they conflict, root AGENTS and COMMON win.

## Read order

1. Root and parent AGENTS, COMMON, this contract, the [promptset index](README.md) and the active stage.
2. [canopen_shared](../canopen_shared/README.md): the seam table, the 01 inventory and the binding
   [02 contract](../canopen_shared/02_profile_integration_contract.md). Cite them; do not restate them.
3. The [pack README](../../reference/moons_canopen/README.md), [review](../../reference/moons_canopen/review.md)
   and [SOURCES.json](../../reference/moons_canopen/SOURCES.json); the
   [CANopen principles pack](../../reference/canopen_principles/principles.md) for generic facts.
4. The family `PROGRESS.md` and the previous stage's report `docs/reports/moons_canopen_<NN>.md` once they exist.
5. Under AGENTS' recurring audit: MotorControl-RS HEAD, dirty state and the seam this stage touches; FieldCore-node
   read-only for ownership changes. A sibling row names an adopted or rejected behavior with path and symbol.

## Repository and consumer boundaries

- Work on `main` with no new branch until version0 is finished (prompt42 instruction in COMMON). Only the lead
  fetches, commits and pushes; workers own disjoint files and run no Git mutation (DELEGATION). Preserve
  concurrent and staged work; stage explicit owned paths; never stash, reset or force-push.
- MotorControl-RS, FieldCore-node and every other sibling stay read-only. No FieldCore integration is claimed.
- Profile code lives in `include/MotorControlCAN/profiles/moons_canopen/` and `src/profiles/moons_canopen/`,
  namespace `MotorControlCAN::MOONS_CANOPEN`, `DriveProfile::MOONS_CANOPEN`, `Manufacturer::MOONS` (added once),
  inventory and JSONL `profile` value `moons_canopen` (canopen_shared 02 "Names and layout"). Stage 02b confirms
  the console family word (proposed `moons`) against the [CLI contract](../../cli_contract.md).
- Core stays C++11, allocation-free, framework-independent and I/O-free, with free functions, fixed value types
  and caller-owned contexts using operation-specific `prepare...`/`next...`/`advance...` and checked getters.
  No heap, clock, sleep, log, thread, retry or replay in the core; no `getResult`, `advanceOperation`, universal
  context or plugin registry. Preparation and parsing publish atomically; a failed call leaves outputs unchanged.
- Applications own the controller, owner, time, queues, caches, node bindings, retries and health policy. One
  owner receives every frame of a physical interface; heartbeat, PDO and EMCY dispatch continue during SDO work.

## Shared seams

- Follow the claim, land, consume rules of the [seam table](../canopen_shared/README.md): recheck it and the Git
  log, claim an unclaimed seam before any shared edit, extend or wait for a claimed one, land each in one
  focused tested commit. Main-sequence seams (S4, S5, S9) are consumed from their prompts only.
- Until S4 (prompt 64) lands, every MOONS' console entry, common-verb dispatch and API/CLI parity test named by
  a stage is Blocked; that stage still delivers its public API, owner-backed tests and ledger rows, stays
  PARTIAL until its console part lands, and records S4 in its inherited-gates column (COMMON).
- A MOONS' binding starts with no best-effort stop shape (S3) and `SdoReuse` `NONE` (S10); stage 04 and the
  ledger's marker review may change them with evidence.
- Seam S13, SYNC and NMT-broadcast effect admission, gates prompt 51's SYNC producer and node-0 NMT (at the
  owner's NMT admission) on every interface with a MOONS' binding; its default deny lands with S2. Stage 09 claims and lands it if the table shows it unclaimed, otherwise extends or waits for the
  claiming family; stages 10 and 12 consume the landed artifact. S11 and S12 are not needed by MOONS' (no
  heartbeat consumer, no node guarding; see the [README](README.md)).

## Evidence and supported behavior

- Original PDF pages and EDS sections are authoritative for what is documented; extracts are an intake aid.
  Reopen the cited page (rendered when a table, figure or bit layout matters) before turning a claim into code.
  Cite `UM21 p58` style with one-based PDF pages and `EDS-STF [6064]` style for EDS sections.
- Every open fact carries its MO-Dnn (or a new ID added by stage 01) and is resolved only by cited new evidence
  or a recorded fixture measurement, in the stage that owns it. Never resolve it by assumption, by a generic
  CiA 402 definition, by another vendor's manual, by an EDS default or by a public library's behavior.
- The 2021 layout (UM21, EDS-STF, EDS-SSDC) and the 2010 layout (AMP10, EDS-TSM) are separate ledger scopes and
  separate generated tables. No entry, constant, unit or test fixture serves both. Index 0x2100, 0x2300, 0x2001
  and 0x7001 are examples of equal or neighboring numbers with different meanings (MO-D02).
- An RW flag, an EDS `PDOMapping=1` or a listed default grants no write. No EDS default is written at
  initialization. A documented object without reviewed units, range and effect is read-only until reviewed.
  The one exception, by the lead's decision of 2026-11-19 ([audit application](audit_application.md), round 2
  decision 4): an explicit write of the documented LED-mask function of 0x2610 and 0x221A is admitted while
  its protection effect is unreviewed; the stage 02 ledger row records that effect as open and every result
  states "protection effect unreviewed (UM21 p85)" (stage 08 item 4). No other object is written on that basis.
- Every MOONS' operation follows canopen_shared 02 "Operation admission": the write preflight of every download
  it may issue against stage 02's `review.write_policy` before its first changing step (enable and partial setup
  included), one absolute operation deadline captured at preparation, and a source-state and intent table
  before the first TX of enable, release, a stop, fault clear or a mode change. Stages name only their MOONS'
  rows and tests; an operation with an unresolved write is refused whole, with nothing sent.
- Unsupported, unqualified or ambiguous operations fail with `UNSUPPORTED` (or the operation's documented error
  detail) before admission and before any TX. No success stub, placeholder API or raw-object escape hatch
  counts as coverage. Preserve independently useful subsets when another operation stays gated.
- Keep documented, implemented, host-tested, firmware-built and hardware-qualified as separate ledger columns.

## Identity and applicability

- No universal identity probe. All three EDS files declare 0x1000 = 0x00060192 and 0x1018:01 = 0x000002D9;
  product code and revision are 0 in every EDS and manufacturer-specific on the drive (UM21 p48). A matching
  reply proves a responding MOONS'-branded CiA 402 server, not the product, layout or firmware.
- The application declares the product row (STF-C, SSDC-C, TSM/TXM-C), node ID and bitrate per binding. The
  declared binding selects the profile and product; product identity only confirms it (MULTIDRIVE row 57).
  Explicit identity reads (stage 03) give prompt 57's outcomes, and its admission by outcome decides
  dependent work, stops exempt as stage 04 item 9 states. This is the
  intra-family form of backlog D18: no identity-based auto-selection, and no vendor-object write without the
  declared model and an admitting outcome for it (prompt 57's admission by outcome, stage 03 item 2); stage
  13 names its further layout gates separately.
- Firmware is reported, not a lock ([prompt 57](../57_regular_drive_operations.md) identity policy and its
  2026-11-12 user requirement). Which object carries "310A"/"310B" is unknown (MO-D01), so an unknown or
  unqualified firmware string never refuses an operation: its terminal result carries "firmware unqualified"
  with the raw 0x100A and 0x2031 strings, and no hardware-qualified claim is made without a firmware
  qualification. Only recorded evidence of a firmware line below 310A (STF) or 310B (SSDC), which UM21 p6
  excludes, refuses the UM21 operations. Stage 03's admission table separates bootstrap reads, layout and
  series evidence, the exact model (declared only) and firmware.

## Owner and bus rules

- One owner per physical interface; one protocol domain per interface unless prompt 63 admits sharing.
- No implicit enable, mode change, homing, fault clear, alarm clear, output change, Q program start, save,
  restore, node-ID or bitrate change, PDO remap, NMT start/stop/reset or watchdog change on startup, binding,
  probe, read, recovery or selection. Each is an explicit caller operation with its own evidence.
- CAN TX success or an SDO download response is not drive acceptance or motion completion. An unconfirmed write
  keeps `UNKNOWN` execution; nothing is replayed automatically. Local cancel or recovery is not a stop. An NMT
  stop is not a quick stop. Host heartbeat observation proves no drive reaction to host loss.
- A SYNC is a bus-wide motion trigger for any MOONS' node in mode -2 (UM21 p36). Until S13 has landed and its
  consumer gate passed, no test, example or console route configures a MOONS' binding and a SYNC schedule on
  the same interface or sends node-0 NMT there; every deliverable that needs both is Blocked, and no family code substitutes for S13.
- Never send RTR, LSS or node-guarding frames to a MOONS' node (UM21 p53; EDS `LSS_Supported=0`; MO-D12).

## Verification commands

Inspect `.github/workflows/verify.yml` and [verification.md](../../verification.md) first and reuse their pins.

```sh
python scripts/check_promptset.py
python scripts/check_promptset.py --check-git-ref INDEX   # before a commit touching references or ledgers; HEAD after
git diff --check
python scripts/check_moons_canopen_ledger.py               # created by stage 01/02
python scripts/generate_moons_canopen_objects.py --check   # created by stage 02; --check never writes
python scripts/check_moons_canopen_operations.py           # created by stage 01
python scripts/check_repository.py
python scripts/verify.py --mode quick
python scripts/verify.py --mode full --compiler gcc --build-dir .build/moons
python scripts/check_firmware.py --target all --idf-path <pinned IDF 5.5.5>  # side-by-side C:\esp\v5.5.5; --idf-python per verification.md
```

Run focused tests for the change first. A new public API, seam, ledger checker registration or build change is
a major change: verify required CI on the exact final pushed commit and report terminal results. Documentation
and promptset edits need local checks only. Never present an older green run as evidence for a new commit.

## Hardware

No MOONS' fixture exists. Never send family frames to the CL86-C bench (COM15, nodes 1 and 2), and never open
COM15 for family work. While COMMON records the owner as away, family hardware work waits for the recorded return
(canopen_shared 02 "Bench steps and owner absence"); the waiver establishes no family fixture. Before first
power establish board, transceiver, drive model (label and part number), motor, wiring, node and bitrate
switch positions, RS-232 tool disconnected (UM21 p9) and applicable authorization, and derive the forbidden
and allowed object lists from the family ledger. The firmware strings and the node ID's 0x2020 part are
established by 14a Q1-Q3's read-only sequence (stage 03's probe and bootstrap reads, then 0x2020 under its
layout admission), the only bench steps admitted before them; every later bench step needs them recorded.

COMMON's owner-absence rule 2 lists the CL86-C fixture's object numbers; they neither forbid nor allow anything
on a MOONS' node. MOONS' persistent and communication-changing objects are its own and come from the stage 02
ledger, at least: 0x1010 store and 0x1011 restore (UM21 p45-p47), 0x2020 node-ID upper bits and 0x2021 bitrate
(UM21 p68), the SYNC and PDO communication and mapping entries 0x1005 and 0x1400-0x1A03 (UM21 p43, p50-p54),
0x1017 heartbeat period and 0x1029 error behavior (UM21 p48-p49), and the 0x2060 watchdog (UM21 p70), plus NMT
reset node and reset communication.

## Handoff and progress

Each stage writes `docs/reports/moons_canopen_<NN>.md` and updates `docs/prompts/moons_canopen/PROGRESS.md`
(created by stage 01). Software states, as in canopen_shared 02: NOT STARTED, IN PROGRESS, SOFTWARE COMPLETE,
PARTIAL or BLOCKED. Hardware evidence sits in its own column (NOT TESTED, NOT RUN with reason, or the exact
fixture and report). The handoff gives:

1. Commits and push result with a fresh ahead/behind check; MotorControl-RS and FieldCore rows.
2. Operations implemented versus gated, by ledger key, with each MO-Dnn disposition and remaining evidence.
3. Local checks and exact-commit CI, including checks not run or blocked, with reasons.
4. Hardware NOT RUN or exact fixture, firmware strings and raw evidence; no family-wide promotion.
5. API, CLI, coverage, backlog and roadmap changes; seams landed or extended; the DOCUMENTATION and
   MAINTAINABILITY records; next-stage prerequisites.
