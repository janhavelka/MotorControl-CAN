# Application of the Lichuan C-family extensions audit, 19 November 2026

Each finding of [audit `aud-20261119-180845-a43848`](audit.md) was checked against this set's
files at `0ab4e181` (`main`), the [LX review](../../reference/lichuan_c_extensions/review.md), the
rendered original pages or EDS lines named below, the shared contract as revised in `1043608b`
([README](../canopen_shared/README.md) seams,
[02](../canopen_shared/02_profile_integration_contract.md) "Operation admission", "First-contact
questions", S13) and [prompt 57](../57_regular_drive_operations.md)'s Identity policy as tightened in
`0ab4e181` (CONTRADICTED refuses motion and every changing request; CONFIRMED is unique among the
supplied references). Following the user's direction of 2026-11-19, every high finding was verified and
fixed, the two medium ones were cheap and clearly right, and nothing outside the findings was added. Only
files in this directory changed; the LX pack, the shared contract, prompt 57, code and tests were not
edited. No hardware, port or build was used.

Dispositions: 11 accepted, 1 accepted in part, none rejected or deferred.

## Findings

| Finding | Disposition | Evidence checked | Change |
| --- | --- | --- | --- |
| F1 exact-firmware lock | Accepted | Old 03b lines 14-16 and 04b line 16; prompt 57 Identity policy (firmware reported, never refuses; never produces CONTRADICTED); `Position.h` `MoveEvidence::firmware` ("must equal the current identity read") and the private `sameFirmware` checks in `Position.cpp`, `Velocity.cpp` and `Homing.cpp` | 03b item 1 defines once, for both models: a same-model record, product fields equal, firmware differences reported as "firmware unqualified" with both identities and never refusing; a missing behavioral fact refuses with its own gate; new models never inherit `sameFirmware`, which CL86-C/OL86-C keep until prompt 57. 04b item 1 consumes it. Gates X15-X17 added (02). Paired tests in 03b and 04b. README non-transfer row for `MoveFirmware`, `VelocityFirmware`, `HomeFirmware`. Widths reported, not deciding (03a item 1, 04a item 2). 07b gate closure records the firmware measured |
| F2 overstated CONFIRMED | Accepted in part | Old 03a lines 45-47, 01 lines 64-66, README lines 98-100; prompt 57 CONFIRMED wording; LX review lines 112-114; EDS bytes: `[2000]` DefaultValue 1 in C-EDS l.1229, TLEDS l.1324 and DSCEDS l.1212 (`lichuan_ds2_canopen/vendor/LC_DSServoCAN_V1.1.eds`); Q01 in `lichuan_objects.json`: both CL86-C specimens read `2000` = 0x0400 | 03a item 5: a future `2401..2407` writer stays blocked until its own stage names independent deployment evidence of the model binding, in addition to CONFIRMED; the residual (an unmeasured TL-C equal to the CL86-C reference is CONFIRMED as CL86-C) is stated. 01 item 6 limit and item 7 register: CL86-C/OL86-C admission is a compatibility choice with its residual risk (`2300` save included); prompt 57's vendor-write policy unchanged. 01 tests: missing, equal and apparently unique references with exact reasons. **Not adopted as stated:** splitting `2000` as a two-EDS claim; the DS-C EDS declares the same default, so the README now cites all three EDS lines and adds that these are declared defaults (live CL86-C value 0x0400) |
| F3 first-contact bootstrap | Accepted | Old 03b lines 20-29 and 39-41, 07a lines 37 and 60-62; shared 02 "First-contact questions" (never opens a gate); `Actions.h` `EnableGate::ATTENDED_EXPERIMENT` (one attended trial with an independent drive-power cutoff, qualifies nothing); `Homing.h` `HomeGate` has no experiment value; `Velocity.h` `VelocityGate::ATTENDED_EXPERIMENT` rests on the C17 order, so it is not extended | First-measurement exception removed from 03b item 3 and 04b item 3. 03b/04b item 2 admit `ATTENDED_EXPERIMENT` under its existing terms (07a T5/K5 route) and prompt 57's measured gate with a model record. 07a gains "Evidence paths" (cites the shared section; production route or assigned isolated experiment, otherwise NOT RUN) and every T/K row names its path; T4 and K4 transitions, T6, T8, T9's `2301` read, K3, K7 and K8's code 4 are experiments. 07a delivers observations only, every gate open; 07b or a later reviewed change closes one. README 07a row |
| F4 SYNC and shared bus | Accepted | Old 06 lines 36-37 and README 125-127; shared README S13 row ("refuses only on a foreign SYNC producer"); shared 02 S13 bullet and "Multi-profile consumers and package scope"; AGENTS shared-bus rule | README S13 bullet and execution contract "Owner and bus" consume S13 and the shared analysis. 06 item 6: the mixed test is library behavior on a recorded synthetic configuration and qualifies no physical bus (CL2 COB-IDs, X09, block a real analysis); before S13 the application refuses a TL-C/CL2 declaration on an interface with an armed schedule and arming one where such a model is declared; after S13 its guard applies; new tests (C-originated schedule, SYNC aimed at the CL86-C node, conflicting routes, broadcast NMT) assert interface traffic |
| F5 CL2 `1018` read | Accepted | Old 04a lines 14-21 and 78-80, 07a lines 49 and 55; CL257 p14..15 (1000h group: `1000`, `1009`, `100A`, `1600..1603`, `1A00..1A03` only; `603F`, `6041`, `6061`, `6064`, `606C` on p19, `60FD` p20); `Reads.h` `StateBlock::ERRORS` (`1001` and `603F`) and `ERROR_HISTORY` (`1003`) | Chosen policy: a caller-selected identity option, off by default, the one admitted undocumented read; explicit single uploads, raw reply or abort, no marker, retry or download, no model from an abort (04a item 2). CL2 read plans hold only CL2 rows: errors without `1001`, error history refused (04a item 1). Tests with the option off and on; acceptance names the one exception. 07a K2 uses the option, K3 (now including `1001`) is an experiment after the owner accepts the read risk, K9 reads `603F` only |
| F6 03a zero-TX wording | Accepted | Old 03a lines 33-35, 54-56 and 77-79; 03b item 11's fresh `2402`/`2407` reads | 03a Deliver and Acceptance state the oracle by direction: no download planned; reviewed uploads emit exactly their request frames; refused setters, restore plans and map mutations emit nothing |
| F7 CL2 index homing | Accepted | Old 04b lines 40-42 and 76-77, 07a line 53; rendered CL257 p6 (EB±/EZ± "contact the manufacturer if an ABZ differential encoder is needed"); ABZ57 p6 extract (Z may be left unconnected); LX-D26, LX-D34 | 04b item 6: index methods refused on CL2-57C/86C even with a declaration until source evidence and X11 qualification exist; on ABZ a Z fixture record is required; switch and start-direction prerequisites checked before setup or trigger with no TX. 04b tests and 07a K7 (NOT RUN on CL2-57C/86C) follow |
| F8 route after save | Accepted | Old 04b lines 60-63 and 78, 07a line 41, 03b lines 59-60; rendered TL57OP p5 (SW5 ON: bitrate through `2407-00` or the debug software, effective after save and restart); rendered CL257 p7 (node table starts at 1) and p8 (the panel drawing labels SW1..SW5 all OFF "自定义", custom) | 03b item 11 and 04b item 11 separate "no node or bitrate write was sent" from "the device answers on this route after restart": the result keeps the obligation on success and on a lost acknowledgement (new gate X18), a restart invalidates dependent evidence, the next session reacquires explicitly without replay. 07a T10 and 06 item 6 follow; tests in 03b/04b. While checking, LX-D32's "no all-OFF row" proved incomplete: 04a item 10 records the intake correction (old claim, p8 evidence, CL2-57C scope, stays open) without editing the LX review |
| F9 completion evidence | Accepted | Old 04a lines 26-35, 04b lines 29-31 and 50, 03a lines 27-28 and 40-41; `Position.h` `nextMove`/`beginMoveObservation` (repeated MONITORS, STATUS, FEEDBACK cycles); shared 02 "One deadline" | 03b item 1 "Observation source", consumed by 04b item 1: samples only from the model-taking getter, bound to model, operation, trigger, mode and generations; observation cycles are polling and need the model's read acceptance, otherwise completion and standstill stay `UNKNOWN`; no TPDO route; the one deadline bounds polling. 03a items 3-4: no legacy getter for TL-C operations, pairing always. 04a item 4: `6041` and `6061` are separate uploads, with the generation and mode rule. Position clear (03b/04b item 8) uses a stationary admission over the model's own observations. Tests for pre-high, mode or generation change, stale or mixed samples, missing transition, fault, no acceptance and expiry |
| F10 stale outcome copy | Accepted | Old 01 lines 44-46, 67-69 and 91-93; AGENTS invalidation rule; `Position.h` `nextMove`/`advanceMove` take the live reservation and current target, and `validateMoveAdmission` is rerun by the transmit guard at hand-off | 01 item 5: the application's `ModelBinding` is the holder; each `next...` and the owner transmit guard reread it and compare generation and session; a cleared, stale or new CONTRADICTED outcome withdraws the pending download unsent, other nodes untouched, a handed-off frame possibly sent; the preparation copy is a snapshot for reporting. Item 7 passes the live holder. Tests between preparation and dispatch and between two changing steps |
| F11 57C model keys | Accepted | Old 02 lines 36-37; 01 line 29; 05 lines 8-10; Q01's `models` already holds designation strings (`CL86-C`, `OL86-C`, `CL57-C`, `OL57-C`) | 02 item 5: `models` holds documentary designations with an optional runtime mapping; item 8 generates only from mapped rows; checker case for an EN57 row with coverage and no selector or admission; 05 Deliver uses it |
| F12 gates and dispositions | Accepted | Old 02 lines 50-65, 03b lines 84-85, 06 lines 42-45 and 62-63; prompt 57 (release rows are CL86-C measurements; mode select for modes 1, 3, 6); TLCEN p10..14 and CL257 p11..13 document modes 1, 3 and 6 | 02 adds X15 (enable, release and stop source states), X16 (move and velocity signals, relative basis and scale), X17 (TL-C homing per method), X18 (save and restore persistence and route) and a total disposition map (ADMITTED, GATED, NOT_DOCUMENTED, NOT_IMPLEMENTED, MISSING_PREREQUISITE; proposed names) landed before the operation stages, with a checker case. 03b item 12 and 04b item 13 give release and mode select for both landing orders of prompt 57. 03b/04b acceptance and 06 items 3, 7 and acceptance print the disposition |

The brief's request to replace "working tree only / not committed" wording for the CANopen
principles pack needed no change here: this set already links the committed pack with normal links.

## Unresolved questions

1. **Answered by prompt 57, now written in stage 01 item 6.** A reference matches when every
   product identity field of prompt 57 (`1000`, `1018:01`, `1018:02`, `2000`) that it holds equals
   the observation's; a held field the observation lacks (not read, aborted, wrong width) leaves the
   reference undecided, so the outcome is at most UNCONFIRMED. Firmware fields and their reply
   widths, the `1018:00` count, the `1018:04` serial and provenance are reported and never decide.
   An unread CL2 `1018` therefore never contradicts, and UNCONFIRMED grants nothing by itself: each
   operation still needs its row, evidence and, for a vendor download, a current outcome.
2. **Partly answered by the code; the rest needs a lead decision.** `CanBusOwner::setSdoReuse` works
   per configured channel binding, and "the default, before any call, is SdoReuse()" with markers
   `1000:00` and `1018:01..03` (`adapters/owner/CanBusOwner.h`); a copy getter exists. Profile
   preparations are I/O-free and never see owner state, so no core entry point can enforce the
   setting. 03a item 2 and 04a item 3 now require it before the first transfer and after every
   rebinding, the example application checks it through the copy getter with a rebinding test, and
   Doxygen states the obligation. An owner-enforced boundary for other applications (for example S10
   keyed by the declared model) is a shared-contract change.
3. **Answered by COMMON and PROGRESS.** The fixture is two CL86-C drives on COM15, and prompt 48
   records OL86-C fixtures as unavailable. OL86-C is a supported API model, not a bench target;
   06 item 5 now admits CL86-C only.

## Reviewer coverage gaps

- Original pages: this application rendered CL257 p6, p7, p8 and TL57OP p5 and read the three EDS
  `[2000]` sections; the other device claims it touched were checked in the extracts against the
  review's page citations.
- Q01: `lichuan_objects.json` Q01 and
  [the 2026-11-02 live connection report](../../reports/2026-11-02_s2_live_connection.md) line 72
  both record `1018:00` U8 = 5 on both CL86-C nodes, as stage 01 item 9 says.
- Code, landing state and hardware: unchanged by this application; stage 01's inventory refresh
  and each stage's checks remain the place to verify them.

## Decisions for the lead or owner

- Admitting `EnableGate::ATTENDED_EXPERIMENT` for TL-C and CL2 (03b and 04b item 2) as the first
  enable route; the alternative is an assigned isolated experiment for T5 and K5.
- The pre-S13 application isolation in 06 item 6 refuses SYNC arming and new-model declarations; it
  admits nothing, but the lead should confirm it is not read as a family-local S13.
- An enforced SDO reuse boundary (question 2).
- The LX-D32 correction is recorded in 04a item 10; annotating the LX review itself is the pack
  owner's choice.
- Gates X15-X18 and the disposition names are proposals; renumber only before landing.

Checks: `python scripts/check_promptset.py` reports no errors for this directory; `git diff --check`
is clean for it. Nothing was compiled; no hardware or port was used; nothing was committed.

## Round 2: audit `aud-20261119-184724-01623c`, 19 November 2026

Each finding of the [second report](audit.md) was checked against this set's files at `bd2b30d9` (unchanged
at `c2a2882a`), the [LX review](../../reference/lichuan_c_extensions/review.md), the rendered pages, extracts,
EDS lines and code named below, and the texts that lead commit `a7656efb` changed for the cross-cutting part:
prompt 57's Identity policy (stop-exemption scope, "Admission by outcome"), canopen_shared 02 "Identity and
device evidence", "Operation admission" (timed runs), S3 and S13, and
[canopen_shared audit_application](../canopen_shared/audit_application.md) "Round 3", whose themes 1 to 5
cover F1, F3, F6, F2 and F4. Those five are applied here by citing the shared rule and adding only
model-specific facts. Following the user's direction of 2026-11-19, every high finding was verified and fixed;
the medium one was fixed where cheap and its remaining part deferred. Only files in this directory changed;
prompt 57, the shared contract, the LX pack, code and tests were not edited. No hardware, port or build was used.

Dispositions: 8 accepted, 1 accepted in part (F5, with its owner-enforced part deferred), none rejected.

| Finding | Disposition | Evidence checked | Change |
| --- | --- | --- | --- |
| F1 stops under a refused identity | Accepted | Old 01 lines 50-52, 03b lines 12-15, 04b lines 13-18; prompt 57 motion rule after `a7656efb` (the exemption removes only the identity refusal; route, binding-generation and source-state checks stay; a later outcome never withdraws a prepared or queued stop; other pending changing steps retire; an NMT command is never that stop) | 01 item 5: a cleared, stale or no longer admitting outcome retires every pending changing step except prompt 57's exempt stop (03b and 04b item 3's reviewed stops), which keeps its own checks and claims no standstill. 03b and 04b item 1 consume the exemption by citation. 03b and 04b Deliver test a contradiction before preparation and after queueing; 01's test list points there because 01 admits no new-model stop |
| F2 best-effort stop, unknown predecessor | Accepted | Old 03b lines 43-51, 04b lines 28-30; canopen_shared 02 S3 bullet and README row (a shape establishes no predecessor; opt in only with a review of the word from every state reachable after an uncertain write, otherwise default deny); TLCEN p11 and CL257 p12 document 15 to 11 only; `Actions.h` `prepareBestEffortStop` takes a `ReadTarget` and no model; `CanBusOwner::admitBestEffortStop` checks the shape only | 03b item 3 separates ordinary stop admission from the best-effort word and keeps S3's default deny for TL-C until a reviewed TL-C decision (X15): `prepareBestEffortStop` takes the live `ModelBinding` and refuses, the example path too, and after S3 lands the application configures no shape; no parallel transmitter; DEC-2 unchanged for CL86-C/OL86-C. 04b item 3 the same for CL2. README S3 bullet. Tests: unresolved route after a possibly sent controlword, stale binding, legacy best-effort tests unchanged |
| F3 identity precedence | Accepted | Old execution contract lines 90-92 ("an outcome other than CONTRADICTED admits a vendor-object download") and 01 item 7 ("any value but CONTRADICTED"); prompt 57 "Admission by outcome"; canopen_shared 02 identity bullets; TLEDS l.83, l.91 (`1018:01/02`); 04a item 2 (CL2 `1018` optional) | Execution contract and 01 item 7 consume prompt 57's precedence for vendor downloads and every changing request, stops exempt: a new-model binding needs a MEASURED record of its own model first. Model-specific addition in 01 item 6: minimum record content (`1000`, `2000`, and `1018:01/02` where the model's sources declare them; for CL2 only when answered), so an empty record cannot match everything. 01 tests: definite mismatch with no other match, no declared-model reference, unread product field and no current outcome refused; a collision including the declared model admitted with every match named; firmware-only change unchanged; a record below the minimum rejected. README prompt-57 bullet and 01 acceptance |
| F4 interface-wide SYNC and NMT | Accepted | Old 06 lines 43-51, README S13 bullet, execution contract line 99; canopen_shared 02 S13 bullet after `a7656efb`: its default deny applies "on an interface with any non-Lichuan binding" through the S2 lookup, so it never sees a TL-C or CL2 binding, both of which carry `LICHUAN_C` | 03a item 6, the first stage in which a TL-C binding carries traffic (04a item 1 cites it for CL2): the example application refuses `armSync`, every individual `admitSync` whatever node it names and every node-0 NMT command on an interface holding a TL-C or CL2 binding, and a new-model declaration while a schedule is armed; retired only when S13's full gate lands with this set's effects supplied as unknown. 06 item 6 exercises it with three models and now refuses broadcast NMT instead of admitting it on per-model rows. 03a acceptance, README S13 bullet and execution contract "Owner and bus" follow |
| F5 SDO reuse markers for other callers | Accepted in part | Old 03a item 2, 04a item 3 and round 1 question 2; `CanBusOwner.h` `setSdoReuse` (every new binding starts with `ACCEPTED_RISK_WITH_MARKER`, markers `1000:00`, `1018:01..03`) and the `sdoReuse` copy getter; `Reads.h` `ReadTarget::channel` (binding and configuration epochs) | 01 item 5: `ModelBinding` holds the application's reuse attestation (the channel epoch for which `NONE` was read back); rebinding clears it; every new-model preparation that takes the binding refuses without a current one, so a forgotten setup or a rebind fails closed on those routes, and the example application checks its model-less probe, identity and read routes the same way. 03a item 2, 04a item 3 and the README S10 bullet; tests for a missing and a stale attestation. **Deferred:** an owner-enforced boundary for other applications' model-less reads and probes and raw owner use, for example S10 keyed by the declared model, is a shared-contract change outside this directory |
| F6 finite run after acknowledgement-only completion | Accepted | Old 03b lines 28-32 and item 5, 04b items 1 and 5; canopen_shared 02 timed-runs bullet and the design it cites (`VelocityOperation.h` `FiniteVelocityRequest::durationUs`; `profiles/lichuan_c/Velocity.h` `requiredStopBudgetUs`, `needsStop`, `STOPPING`); LX review "Limits and protection" (host loss documented nowhere) | 03b item 1: ending without observations retires no stop obligation. 03b item 5: an admitted TL-C run is a timed run under the shared bullet; preparation refuses unless item 3's stop table admits every state the run can leave, a lost start reply included; an urgent stop that cannot be admitted (unresolved route, best-effort denied) keeps `needsStop` and motion unknown. 04b item 5 cites it. Timed-run tests with a synthetic record in 03b and 04b. 07b physical item 3: finite velocity qualifies only with measured start-to-stop criteria per run |
| F7 CL2 mode behind a statusword | Accepted | Old 04a lines 37-48, 04b lines 34-36; CL257 p12 and p14 (bit 10 by mode, `6061` listed) | 04a item 4: every CL2 status acquisition brackets the statusword (`6061`, `6041`, `6061`) under the operation's reservation; bit 10 means something only when both mode replies equal the commanded mode and all three share node generation and server session, otherwise `UNKNOWN`; the residual (a mode changed and restored by another master or the vendor software between the reads) goes into Doxygen. 04b items 1 and 4 consume it. Tests: the reviewer's interleaving (04a) and an acknowledged `6060` whose display lags the sample (04b) |
| F8 first-contact prerequisites | Accepted | Old 07a T1, T5, K2, K5 and Deliver, 06 item 2 (static configuration, no entry route), 03b item 2, 04b item 2; `Actions.h` `EnableGate::ATTENDED_EXPERIMENT` and `prepareEnable` (fresh read in mode 1 with reviewed `607A` and `60FF`; never changes mode or zeroes a target); `Position.h` `preparePositionParameters` (arithmetic only, writes nothing); TLEDS l.1083..1088 (`6060` default 0), l.1096 (`6061` 0), l.1119 (`607A` 5000); LX review "Units and scaling" (TLO position not measured) | T1 and K2 separate capture (the production read after 03a/04a) from loading: a record becomes a MEASURED reference only through the application's static configuration, the one loading path, defined and tested in 06 item 2 (minimum content checked at load, no console write). 07a "Evidence paths": every changing production route needs that record first (prompt 57's precedence). T5 and K5 name their preconditions, observed in one fresh state read and never written: an admitted starting state, `6061` = 1, `607A` equal to `6064` (a controller report on TLO) and a stop from predecessor 15 prepared; the TLEDS defaults would fail both checks; otherwise experiment or NOT RUN; an enable that stops before word 15 sends nothing further and leaves the independent power cutoff. 03b and 04b item 2 say the preconditions are observed |
| F9 electrical ambiguities before first power | Accepted | Old 07a lines 20-23, 07b lines 36-37; rendered TLCEN p6 (pins 1 VDC and 2 GND both "DC Power Negative Terminal"; 24~36, 24~50, 24-70 V), rendered TLCEN p7 (figure DO1/DO2 alarm and brake release; DC18-36, DC24-48, DC24-48 V; note 3 DOCOM to 0 V only, while p6 lists no DOCOM pin), rendered TL57OP p4 (DO1/DO2 with brake output on DO2, DI 12~24 V); CL286 p5 extract (both main terminals "AC", DC30-110 V or AC20-80 V, no DC polarity); LX-D09, LX-D10, LX-D14, LX-D31 | 07a "Before first power": a fixture electrical record, reviewed by the owner before first power, states power connection and polarity, a supply inside every documented range for the frame size, and every connected output, common terminal and brake release, each from the unit's own marking or the vendor's written clarification, never by choosing between conflicting texts; outputs stay unconnected and a braked motor unused until resolved; without the record every bench step is NOT RUN. 07a Deliver and Acceptance, 07b physical item 1 (X14 keeps the `2502` current bounds), execution contract "Hardware" and the README 07a row follow |

### Unresolved questions

1. **TL-C position-clear stationarity: answered by the code, now written in 03b item 8.** The C path's own
   Doxygen (`Actions.h` `prepareSetDevicePosition`: mode 1, `2100` running and alarm clear, `606C` zero,
   "conservative stationarity evidence, not an observed shaft") makes the predicate a controller report.
   03b item 8 now applies that conservative set to TL-C reports: one fresh read through the model-taking
   getter with mode 1, bit3 alarm and bit14 running clear (TLCEN p11) and `606C` zero (listed on TLCEN p15).
   It is a controller-reported predicate: on TLO `6064` and `606C` are not measured feedback, and with X04 open
   the bits' freshness is unqualified, so the result names X04 and claims no standstill. A physical standstill,
   where a test needs one, is the attendant's observation. 04b item 8 says the same for CL2.
2. **Prompt 57 landing after closeout: answered in part by the files, now enforced here.** 03b item 12 and 04b
   item 13 already gave the dispositions for both landing orders, and the set's rule for the identity
   comparison is that whichever lands later extends it. Stage 02 item 9 now adds a checker rule: a
   NOT_IMPLEMENTED cell names its dependency by public function, and once that function is in the operation
   map the checker fails until the cell is decided again, so the commit that lands prompt 57 also updates the
   TL-C and CL2 cells, their console tests and coverage rows (03b item 12, 04b item 13, 07b item 1). Prompt
   57's own deliverables do not name this update; adding that line is outside this directory.

### Reviewer coverage gaps

- Original pages: this application rendered TLCEN p6 and p7 and TL57OP p4, read the CL286 p5 extract and
  TLEDS l.54..107 and l.1083..1119; the other device claims it touched were checked in the extracts against
  the review's citations.
- Code: read only for the symbols named above; landing state, builds and hardware are unchanged.

### Decisions for the lead or owner

- S13's default deny keys on non-Lichuan bindings, so TL-C and CL2 bindings escape it. This set's
  application isolation (03a item 6) fills the gap for the example application; other applications have a
  Doxygen obligation. An owner-level deny keyed by the declared model, and a full S13 gate that evaluates
  Lichuan effects per declared model, are shared-contract changes.
- S3: a Lichuan binding keeps DEC-2's shape by default. 03b and 04b item 3 need the application to configure
  no shape for a TL-C or CL2 binding, which S3's per-binding configuration must allow for a Lichuan binding
  (or key its default by declared model).
- An owner-enforced SDO reuse boundary (F5's deferred part, round 1 question 2).
- Prompt 57's deliverables could name the update of this set's release and mode-select cells (question 2);
  stage 02's checker rule enforces it either way.
- Proposals to confirm: the owner as reviewer of the fixture electrical record, the `ModelBinding` reuse
  attestation, and the bracketed CL2 status acquisition.

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this
directory. Nothing was compiled; no hardware or port was used; nothing was committed.

### Round 2 lead decisions

1. S13 keyed by declared model: not changed. S13's default deny stays keyed on non-Lichuan
   bindings; this set's application rule in 03a item 6 and 04a covers TL-C and CL2 until S13's full
   gate lands and consumes their unknown effects. One interim rule, owned here, is simpler than a
   model key in the S2 commit.
2. S3 with no shape for a Lichuan binding: not changed. The refusal in `prepareBestEffortStop` for
   TL-C and CL2 (F2) is family-local and sufficient; CL86-C and OL86-C keep DEC-2.
3. S10 owner-enforced reuse boundary (F5's deferred part): stays deferred and open.
4. Prompt 57 deliverable: added to its "Verify and exit" (if this set closes out first, prompt 57's
   landing commit updates the release and mode-select cells).
5. Electrical record before first power (F9): the owner reviews it at the bench; while COMMON records
   the owner as away, no TL-C or CL2 bench step runs in any case.

## Round 3: audit `aud-20261119-191534-a7642a`, 19 November 2026

Each finding of the [third report](audit.md) was checked against this set's files at `83a7a232` (unchanged at
`94138cd7` and `6ff80ed1`), the [LX review](../../reference/lichuan_c_extensions/review.md), the rendered
original pages, extracts and code named below, and the shared changes made since the audit, recorded in
[canopen_shared audit_application](../canopen_shared/audit_application.md) "Round 4" themes 1 to 5: `c4e5574f`
(canopen_shared 02 timed runs need a stop that can still leave on an unresolved route, either an opted-in S3
shape, a native equivalent or an application-declared independent stop named in the result; a stop word keeps a
set halt bit; prompt 57's Stopped-node bootstrap command; S11) and `6ff80ed1` (S3's per-binding configuration
accepts an explicit no shape that overrides the profile default, set before first traffic and kept across rebind,
used by TL-C and CL2). F1 and F3 are applied by consuming those rules. Following the user's direction of
2026-11-19, every high finding was verified and fixed, and the three medium ones were cheap and clearly right.
Only files in this directory changed; the shared contract, prompt 57, the LX pack, code and tests were not
edited. No hardware, port or build was used.

Dispositions: 9 accepted, 1 accepted in part (F5), none rejected or deferred.

| Finding | Disposition | Evidence checked | Change |
| --- | --- | --- | --- |
| F1 best-effort stop boundary | Accepted | Old 03b lines 52-62, 04b lines 31-33, README S3 bullet; round-2 lead decision 2; canopen_shared 02 S3 after `6ff80ed1`; `CanBusOwner.h` `admitBestEffortStop` (accepts `6040:00=000B` on any binding today) and `SdoClient::checkBestEffortStop` | Round-2 lead decision 2 is superseded. 03b item 3: refusal at two levels. Owner: every TL-C binding carries S3's explicit no shape, set where the application binds a node with a declared TL-C model, before first traffic, beside `SdoReusePolicy::NONE`, and kept across rebind; the example's binding and declaration routes (06 item 2) set and check it. Profile: the `prepareBestEffortStop` refusal stays. Before S3 lands no owner entry point refuses a direct call, so no TL-C changing operation except the reviewed stops is admitted (NOT_IMPLEMENTED, S3 missing). 04b item 3 applies the same to CL2. 03a item 2 names the setup point. Tests: a direct `admitBestEffortStop` after a timeout and after a rebind, `INVALID` with zero TX. README S3 bullet, stage rows, 03b and 04b acceptance |
| F2 queued interface-wide commands | Accepted | Old 03a lines 75-86; `CanBusOwner.h`: `admitSync` copies work ("Local admission only"), hand-off follows through `canTransmitSync`, `holdSyncHandoff` and `noteSyncSubmitted`; `admitNmt` ("No TX occurs during admission"); `cancelSync`, `cancelNmt` keep possible earlier effects | 03a item 6: a TL-C or CL2 declaration or rebinding is also refused while a SYNC or node-0 NMT request is pending (admitted, not terminal). The change runs in the owner's one service context after checking the live schedule and pending requests; the application may cancel first or wait for the terminal; a handed-off frame is reported possibly sent, recorded with the declaration, never recalled. Tests: one-shot SYNC and broadcast NMT admitted before the declaration with hand-off held, then declaration or rebinding (refused, no later TX), then cancellation and declaration (zero SYNC and NMT TX). 06 item 6 lists the case |
| F3 stop path after a lost start reply | Accepted | Old 03b lines 70-79, 04b lines 42-46; canopen_shared 02 timed runs after `c4e5574f`; S10 `NONE`; 03b item 3 (no S3 shape, no native equivalent) | 03b item 5: a stop row is not a stop path; preparation refuses before the first changing TX unless the application declares an independent stop for the run (enable, STO or drive-power path), which the result names; at stop due on an unresolved route the result keeps `needsStop`, reports motion unknown and names that stop. 04b item 5 the same. Tests: no declared stop (refused at preparation); a possibly sent start and a timeout under `NONE` (no CANopen stop TX, declared stop named). The halt-bit rule of `c4e5574f` is consumed in 03b and 04b item 3: halted (`010F`) and unknown-halt predecessors are not predecessor 15, no TL-C or CL2 page documents a quick stop keeping bit 8, so those rows refuse (X15), with tests |
| F4 stop prepared before enable | Accepted | Old 07a T5 and K5; 03b item 3 (ordinary stop needs a fresh admitted predecessor); `Actions.h` `EnableGate::ATTENDED_EXPERIMENT` (independent drive-power cutoff) and `prepareEnable` (607A/60FF re-uploaded before word 6) | 07a T5: before word 6 only stop readiness (a reviewed predecessor-15 stop row, the channel's urgent path free, the power cutoff attended); the stop is prepared only from a fresh read showing predecessor 15 after word 15's acknowledgement. Three partial-enable branches: word 15 known unsent (nothing further, last state recorded, cutoff only); possibly sent with no acknowledgement or observation (no predecessor, no best-effort shape, cutoff, enable state unknown); acknowledged with a fresh predecessor 15 (stop prepared). A plan not covering them leaves T5 an experiment or NOT RUN. K5 takes the same plan |
| F5 position clear on unqualified reports | Accepted in part | Old 03b item 8, 04b item 8; `Actions.h` `prepareSetDevicePosition` (mode 1, `2100` running and alarm clear, `606C` zero: "conservative stationarity evidence, not an observed shaft: a live test also needs that"); X04 and X12 open; LX review "Units and scaling" | 03b item 8 defines admission apart from wording: GATED under X04 until a TL-C status record of that model qualifies bits 3 and 14 after transitions (07a T4, 07b); with it, mode 1, bit3 and bit14 clear and `606C` zero from one fresh model-taking read under the reservation that carries the writes, no earlier operation left motion `UNKNOWN`, `needsStop` set or its route unresolved (a proposed input: the binding's last motion terminal), and mode, generation and session rechecked before each `2302` write. 04b item 8: GATED under X12 until a CL2 monitor record qualifies `2100` bits 2 and 3, mode 1 from the same acquisition added, same exclusions. Tests: GATED with zero TX; stale reports, a move left `UNKNOWN`, an unresolved route, a mode or generation change before dispatch. X04 and X12 rows in 02 note the gate. **Not adopted as stated:** a mandatory independent stationarity observation for every clear. Once a model record qualifies the reports, they describe the drive state that the counter clear depends on; the result still claims no standstill, and a test needing a still shaft keeps the attendant's observation |
| F6 CL2 homing boundary | Accepted | Old 04b item 6, 03b item 6 (X17 per method); LX review lines 259-262 (no halt behavior beyond the 271 example, no host-loss reaction) and "Limits and protection"; CL257 p13 (bit 4 falling interrupts homing); `Homing.h` `needsStop`, `advanceHomeStop` | 04b item 6: each admitted method needs a CL2 homing record for that model and method (X11: validated signal sequence, search bound, admitted interrupt predecessors), otherwise GATED; X11's row in 02 says so. Interruption is defined once in 03b item 6 for both groups: preparation refuses without read acceptance, without a search bound ending before the one deadline, and without a declared independent stop; the application requests the mode-6 HALT when the bound or deadline passes without the expected signal or transition, or observation is lost or expires after a possibly sent trigger; where the HALT cannot be admitted only the declared stop remains and `needsStop`, motion and reference stay unknown. Tests in 03b and 04b. 07b item 6 puts the bound and declared stop in the envelope |
| F7 CL2 `2502` motor limit | Accepted | Old 04b item 10 and 03b item 10; TLCEN p18 text layer ("Maximum Peak Current, Unit: mA"); CL257 p17 and ABZ57 p17 (最大峰值电流, 单位mA, 0~7000), CL286 p17 and ABZ86 p17 (0~8000), located by PDF text search and read in the extracts | 04b item 10: the setter needs both the model's drive range and the caller's X14 record for the attached motor, a positive peak-current limit in mA; a missing or zero limit or a value inside the drive range above it refuses with zero TX. 03b item 10 states the same record for TL-C (whose `2502` has no documented bound) and that the caller, not the library, converts another rating basis. X14 row in 02. Tests in 03b and 04b |
| F8 reuse policy change on the same channel | Accepted | Old 01 item 5, 03a item 2; `CanBusOwner.h` `setSdoReuse` ("A change applies to later resync steps; it does not reopen, re-quarantine or cancel anything"; no epoch advance) | 01 item 5: the attestation also requires the policy to stay `NONE`; any policy change on the binding clears it first and retires the binding's unsent work. 03a item 2: the example application refuses any other `setSdoReuse` on a channel holding a new-model binding. Tests: a same-binding policy change around a queued request, then a timeout and a late reply (no marker, no `acceptedCorrelation`, no replay); 01's applicability test. The raw-caller boundary (round 2 F5, decision 3) stays deferred |
| F9 CL2 consumes TL-C artifacts | Accepted | Old README 04a row, 04a intro, 04a items 1, 3, 4, 04b items 1, 3, 5, 13 | 04a intro names 03a items 2, 3 and 6 as common artifacts and 04b's intro names 03b items 1, 3, 5, 6 and 12: whichever stage of the pair runs first lands them for both groups with their tests. README stage rows for 03a, 03b, 04a and 04b say so; the CL2 rows stay otherwise independent |
| F10 qualification pass threshold | Accepted | Old 07b physical items 3 and 4; AGENTS evidence rules (no invented timing) | 07b physical item 3 (new): every qualification run needs a fixture- and operation-specific pass envelope approved by the owner before the run (observable, limits from caller or fixture review, the independent observation behind any physical claim); without one the run is characterization and qualifies nothing. Item 4: finite velocity's envelope bounds the urgent-stop TX after stop due within the caller's stop budget and the stopping time or travel, measured independently; a run outside it or with an unconfirmed stop fails and reports motion unknown. Acceptance and README 07b row follow; later items renumbered |

### Unresolved questions

1. **Answered by the files, now written in stage 01 item 6.** Prompt 57's UNCONFIRMED covers "fields that
   could not be read", and its CONFIRMED needs every other candidate's reference to be a decided non-match. An
   undecided reference is neither. With a declared-model reference matching and another candidate's
   reference undecided, the outcome is UNCONFIRMED with a declared match, admitted under "Admission by
   outcome", and the result names the undecided reference and its missing field; an undecided declared-model
   reference with no other declared match refuses. This admits and refuses nothing beyond prompt 57. 01
   adds the test.
2. **Answered in part by the code, now written in 07a T5 and K5.** The plan is one fresh state read through
   the model-taking getter under the reservation: `Reads.h` `StateBlock` STATUS (`6041`, `6061`), FEEDBACK
   (`6064`, `606C`), MONITORS and ERRORS, with CL2's `6061` from 04a item 4's bracketed acquisition. The
   reviewed `607A` and `60FF` come from earlier explicit uploads into `EnableEvidence`
   (`reviewedPositionTarget`, `reviewedVelocityTarget`), and `prepareEnable` uploads them again before word 6
   and stops on any difference (`Actions.h`). No file establishes a shared basis: TLCEN p10 lists
   `6064` "Current Position" INTEGER32 with no unit beside `607A`'s -1000000..+1000000 range, the p15
   dictionary gives none either, and CL257 p11 ("当前位置") and p19 match. Raw equality is now a screen, the basis
   stays X16's question, and T5 and K5 rely on the enable sending no trigger, stop readiness and the attended
   power cutoff.

### Reviewer coverage gaps

- Original pages: this application read the original PDFs' text layer (PyMuPDF) for TLCEN p10 and p18 and
  CL257 p11 and p19, located `6064` and `2502` in all TL-C and CL2 PDFs by text search and read the `2502`
  rows of CL286, ABZ57 and ABZ86 in the extracts; no page was rendered as an image this round. Other device
  claims were left as round 2 checked them.
- Code: read only for the symbols named above; landing state, builds and hardware are unchanged.

### Decisions for the lead or owner

- S3 timing: 03b and 04b now admit no TL-C or CL2 changing operation other than a reviewed stop until S3's
  explicit no shape exists. S3 lands with S2, so these stages wait for the first S2 family unless the lead
  lands S3 earlier. 03a and 04a reads can run before S3, and until then a direct `admitBestEffortStop` by an
  application on a new-model binding still sends `000B`; the example application refuses its own path.
- Outside this directory: the canopen_shared README S3 row does not yet mention the explicit no shape that
  `6ff80ed1` added to 02; adding it keeps the seam table and contract consistent.

### Round 3 lead decisions

1. S3 timing: the explicit per-binding no shape is separable from S3's default deny for other profiles. If
   S2 has not landed, 03b item 3 lands that part itself and records the claim in the canopen_shared README;
   the TL-C and CL2 operations therefore wait only for this set's own stage, not for the first S2 family.
2. Homing counts as a run the host ends: a home also needs a stop that can leave on an unresolved route, so
   with no shape it needs the application-declared independent stop (03b items 5 and 6).
3. The canopen_shared README S3 row now names the explicit no shape and its earlier landing.
- Homing now uses the shared timed-run stop-path rule (a declared independent stop), which extends "any run
  the host ends" to a home ended by interruption; confirm.
- Proposals to confirm: position clear GATED under X04 and X12 with the last-motion-terminal input; the
  owner as approver of each qualification envelope (as for the fixture electrical record); the example
  application pinning `NONE` on new-model channels. The owner-enforced S10 boundary stays open (round 2
  decision 3).

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this
directory. Nothing was compiled; no hardware or port was used; nothing was committed.

## Round 4: audit `aud-20261119-193919-6d6362`, 19 November 2026

Each finding of the [fourth report](audit.md) was checked against this set's files at `a6b58cf1` (unchanged at
`85e54d01` and `c138e959`), the [LX review](../../reference/lichuan_c_extensions/review.md), the original pages,
extracts, EDS lines and code named below, and the shared changes made since the audit in `328651b6`, recorded in
[canopen_shared audit_application](../canopen_shared/audit_application.md) "Round 4" themes 6 to 8: with the
halt state unknown a stop word sets bit 8; homing is a host-ended run under canopen_shared 02 "Timed runs"; and
prompt 57's undecided-reference rule. F2 and the unresolved question are applied by citing those rules.
Following the user's direction of 2026-11-19, every high finding was verified and fixed, and the two medium
ones were cheap and clearly right. Only files in this directory changed; the shared contract, prompt 57, the LX
pack, code and tests were not edited. No hardware, port or build was used.

Dispositions: 6 accepted, none accepted in part, rejected or deferred.

| Finding | Disposition | Evidence checked | Change |
| --- | --- | --- | --- |
| F1 no shape before first traffic | Accepted | Old 03a lines 32-33 ("Where canopen_shared S3 has landed"), 04a lines 10-11, 03b lines 72-78; canopen_shared 02 S3 (explicit no shape set before the binding's first traffic, kept across rebind); canopen_shared README S3 row (`validBestEffortStop` accepts `000B` on every channel; the extensions set may land the no shape first); S10 `NONE` (a timeout leaves the route unresolved) | 03a item 2 sets and checks S3's explicit no shape beside `NONE` before the first transfer, reads included, as a checked prerequisite of the first TL-C or CL2 traffic; if it has not landed, whichever of 03a and 04a runs first lands that part (override, retention across rebind, `INVALID` no-TX return, tests; S3's default deny stays with S2) and records the claim; until then no TL-C or CL2 binding carries traffic. New test: a timed-out identity upload, then a direct `admitBestEffortStop`, also after a rebind (`INVALID`, zero TX). 03b item 3 drops its own landing paragraph and the interval in which only reviewed stops were admitted; 04b item 3 cites 04a item 3. 04a intro and item 3, 06 item 2, README S3 bullet and stage rows, and the 03a, 03b, 04a and 04b acceptance follow. This moves round-3 lead decision 1's landing point from 03b to the reads stages |
| F2 homing stop budget and cutoff | Accepted, by citation | Old 03b lines 106-115, 04b lines 71-74, 07b physical item 6; canopen_shared 02 "Timed runs" after `328651b6` (homing: a caller-supplied latest interruption time takes the duration's place, a travel bound may interrupt earlier from observations no older than a caller-supplied age, the same stop budget, transfer cap and stop path) | 03b item 6 consumes it: preparation refuses without read acceptance, without a latest interruption time whose stop budget fits before the one deadline, or without the declared stop; a travel bound acts only on observations within the caller's age, and an older or missing one neither triggers nor extends anything; every later transfer is capped at stop due; the mode-6 HALT goes out as the separate urgent stop with its own deadline; where it cannot, `needsStop`, motion and reference stay unknown until fresh evidence resolves them. 04b item 6 cites it; 07b item 6 puts the time, travel bound and observation age in the envelope. Tests: no time or no room for the budget, an upload in flight at the time, a service cycle resuming after it, a travel bound seen only in a stale sample |
| F3 raw target-at-actual equality | Accepted | Old 03b lines 44-46, 04b lines 26-27, 07a T5 and K5; rendered TLCEN p15 (`6064` "actual position" I32, no unit), TLCEN p10 and CL257 p11 and p19 as round 3 read them; `Actions.h` `EnableEvidence` (`reviewedPositionTarget` "History, not intent"; the C path compares re-uploaded reviewed values, never `607A` with `6064`); LX review "Units and scaling" (TLO has no encoder) | 03b item 2: raw equality is a screen; the attended route also needs the caller's basis record for the fixture (both values on one coordinate basis, scale and origin, from a vendor clarification or an assigned experiment's retained result; X16 stays open), otherwise MISSING_PREREQUISITE with zero TX, also when the raw values are equal. 04b item 2 the same for CL2. 07a T5 and K5: the production route only with that record, otherwise a separately assigned experiment whose plan allows and bounds unexpected motion, or NOT RUN. Tests in 03b and 04b Deliver |
| F4 alarm-clear source states | Accepted | Rendered TLCEN p11 ("15->128 Alarm reset" only in the position-mode table; low bits 111 paired with 6040=15), p15 (`603F` FF01 overcurrent, FF05 excessive position deviation) and p19 (overcurrent, overvoltage and undervoltage "Restart to reset"; memory read/write and position error "Can be reset"); TLCEN p12 extract (no reset row in velocity mode) and p14 (bit 7, "set it back to 0"); TL57ZH p10, p13, p14 and p18 extracts agree; rendered CL257 p12 (15->128, the same low-bit pairing) and p20 (only Err5 "掉电、手动复位"; Err2/Err3 自动恢复; the rest 掉电复位; Err9 also P0D.07); CL257 p14 and p16 extracts (bit 7 then manual 0; P0D-03 0/1) and p19 (`603F` without codes); `Actions.h` `AlarmClearMethod` and `prepareClearAlarm` (C admission `2100` alarm with `603F` FF05 in modes 1 or 3, CL86-C measurements) | 03b item 7: a TL-C table chosen from one fresh model-taking read. Admitted: bit3, FF05, mode 1, low bits 111, bit14 clear, `0x0080` then `0x0000` after its acknowledgement (the FF05-to-p19 link recorded as inference). Refused: FF01 (restart), running set, `OBJECT_2301` (X07). GATED under X07: any other code, mode 3 or 6, other low bits. Already clear: reported, nothing sent. A first or second word possibly sent without acknowledgement sends nothing further and reports bit 7 possibly asserted; no repeat; completion needs a fresh bit3 clear and never claims the cause is gone. 04b item 7: a CL2 table in which both methods are GATED under X13 until a CL2 fault record links the CANopen report to a fault p20 lists as manually resettable (`603F` has no code list), then admitted only from mode 1, low bits 111, running clear; other modes GATED (X13, X15); running refused; already clear reported. X07 and X13 rows in 02. 07a T9 names the admitted row; K9 leaves an alarm clear NOT RUN until its observations give that record. Row tests in 03b and 04b |
| F5 bootstrap reads and NMT | Accepted | Prompt 57 motion rule ("only the bootstrap identity and state reads remain") and "Admission by outcome" (targeted Enter Pre-operational on a fresh Stopped report under every outcome except CONTRADICTED); LX review "Interface and protocol" (TL-C NMT: TLEDS `SimpleBootUpSlave=1`, no manual text; heartbeat: TLEDS `1017` default 0, no `1016`, `100C` or `100D`; CL2: neither documented); TLEDS l.27 and l.236; 03a item 6 and 04a item 1 (heartbeat observations documentary-unqualified or raw; no `1017` setter); `ProbeDevice::requestNmt` (the example's NMT route takes no model and passes any targeted or broadcast command to `admitNmt`) | Execution contract "Routes by outcome (TL-C and CL2)": bootstrap reads (probe, identity with options, state read) under every outcome; ordinary reads admitted except on CONTRADICTED; prompt 57's bootstrap Enter Pre-operational withheld for both models, with the reason (no documented NMT or heartbeat behavior, no reviewed Stopped report), the node reported stranded and its recovery the application's (an owner power cycle), with no other NMT command or broadcast as fallback; every other targeted NMT command refused (NOT_DOCUMENTED); node-0 NMT and SYNC by 03a item 6. A read admitted before the outcome changes completes and admits nothing; no targeted NMT command reaches the owner, so no hand-off case remains. 03a item 6 implements and tests it per outcome, including a fresh synthetic Stopped heartbeat; 04a item 1 applies it; 01 item 7 and the README prompt-57 bullet point to it; 07a "Evidence paths" notes that ordinary reads wait for T1's or K2's record on a CONTRADICTED binding |
| F6 CL2-first ownership | Accepted | Old 03b intro (items 1, 3, 5, 6), 04b intro (items 1, 3, 5, 6 and 13 from 03b 1, 3, 5, 6 and 12), README 03b and 04b rows; 04b item 8 (imports 03b item 8's last-motion terminal and rechecks) and item 10 (imports 03b item 10's record) | 03b and 04b intros and the README rows list 03b items 1, 3, 5, 6, 8, 10 and 12 (04b items 1, 3, 5, 6, 8, 10 and 13), naming item 8's retained last-motion terminal input and rechecks before each write and item 10's caller peak-current record and refusals; whichever of 03b and 04b runs first lands their shared parts with tests. This also fixes 03b's list, which omitted item 12 |

### Shared rules consumed beyond the findings

- Unknown halt state (`328651b6`, theme 6). 03b item 3 refused halted and unknown-halt predecessors because no
  TL-C page documents a quick stop keeping bit 8; canopen_shared 02 "State transitions" now says a stop word sets
  bit 8 when the halt state is unknown and is never refused merely for that. TLCEN p11 and CL257 p12 give
  controlword bit 8 no meaning other than the 271 stop, so 03b and 04b item 3 now send HALT `010F` and the quick
  stop as `010B` from a fresh low-bits-111 report whose halt state is set or unknown, record the halt state and
  that `010B` is undocumented for the model (X15), and the stop-table test asserts those first frames. Round 3's
  "those rows refuse with X15" is superseded.
- `c138e959` landed after the brief and needs no change here: no TL-C or CL2 stop row is admitted without a
  fresh report, so every run and home already needs the declared independent stop (03b item 5), which covers a
  stale, missing or alarmed report at stop due; and no targeted or node-0 NMT command to these models reaches
  the owner's hand-off.

### Unresolved question

1. **Answered by prompt 57 after `328651b6`, now cited in stage 01 item 6.** A reference with a required
   field unread, of the wrong width or aborted is undecided; CONTRADICTED needs every declared-model reference
   decided as non-matching. A competing definite match beside an undecided declared-model reference therefore
   gives UNCONFIRMED without a declared match, which refuses the same changing work; 01 Deliver adds that
   regression. The Stopped-node bootstrap difference does not arise for these models: the route table
   withholds that command under every outcome.

### Reviewer coverage gaps

- Original pages: this application rendered TLCEN p11, p15 and p19 and CL257 p12 and p20 (PyMuPDF, scratch
  images not retained) and read TLCEN p12 and p14, TL57ZH p10, p13, p14 and p18 and CL257 p14, p16 and p19 in
  the extracts, and TLEDS l.27 and l.236; other device claims stay as earlier rounds checked them.
- Code: read only for the symbols named above; landing state, builds and hardware are unchanged.

### Decisions for the lead or owner

- F5: withholding prompt 57's bootstrap Enter Pre-operational for TL-C and CL2, refusing every other
  targeted NMT command for them, and refusing ordinary reads on CONTRADICTED (so first-contact ordinary reads
  wait for T1's or K2's record when only the equal CL86-C record is a candidate). Consuming the bootstrap
  command instead would need a reviewed heartbeat record per model.
- F4: CL2 alarm clear, admitted on documentation before, is now GATED under X13 for both methods; TL-C admits
  only the FF05 mode-1 row with fresh low bits 111, which never applies if the drive drops those bits on an
  alarm, until T9 or a vendor clarification shows otherwise.
- F3: the caller's fixture basis record is a new MISSING_PREREQUISITE input (proposal).
- Halt bit: the undocumented `010B` quick stop is now admitted for TL-C and CL2 under the shared rule; confirm.
- F1 moves the S3 no-shape landing point from 03b to whichever reads stage runs first (round-3 decision 1).

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this
directory. Nothing was compiled; no hardware or port was used; nothing was committed.

### Round 4 lead decisions

1. NMT (F5): withholding prompt 57's bootstrap Enter Pre-operational for TL-C and CL2 is confirmed while no
   model evidence covers their NMT behavior; a Stopped node is reported stranded and its recovery is the
   application's. Ordinary reads refused on CONTRADICTED; first-contact reads wait for the T1/K2 record.
2. Alarm clear (F4): CL2 GATED under X13 for both methods and TL-C's single admitted row (FF05 in mode 1)
   are confirmed.
3. Quick stop with unknown halt: `010B` for TL-C and CL2 is confirmed under canopen_shared 02 "State
   transitions" (setting the CiA 402 halt bit cannot start motion; no LX page gives bit 8 another meaning).
4. No-shape landing (F1): moving it to the first reads stage (03a or 04a) supersedes round-3 decision 1's
   landing point; the reasoning of that decision stands.

## Round 5: audit `aud-20261119-201350-68dc0f`, 19 November 2026

Each finding of the [fifth report](audit.md) was checked against this set's files at `570320bb` (unchanged
from `b661fba7` and at `2d1c70c2`), the [LX review](../../reference/lichuan_c_extensions/review.md), the
extract pages and code named below, and the shared change made after the audit in `2d1c70c2`, recorded in
[canopen_shared audit_application](../canopen_shared/audit_application.md) theme 12: in canopen_shared 02
"Timed runs" the declared independent stop carries a caller-supplied fallback deadline, and the application
invokes it when the urgent stop is not admitted, misses its dispatch window or leaves the stop effect
unconfirmed at that deadline (an early homing interruption with an upload pending included); admission, TX or
an acknowledgement never retire that need. F1 is applied by citing that rule. Following the user's direction
of 2026-11-19, every high finding was verified and fixed, and the three medium ones were cheap and clearly
right. Only files in this directory changed; the shared contract, prompt 57, the LX pack, code and tests were
not edited. No hardware, port or build was used.

Dispositions: 7 accepted, none accepted in part, rejected or deferred. F6's prompt 57 line is outside this
directory and is listed for the lead below.

| Finding | Disposition | Evidence checked | Change |
| --- | --- | --- | --- |
| F1 deadline for the declared stop | Accepted, by citation | Old 03b lines 102-105 and 119-126, 04b lines 76-81; canopen_shared 02 "Timed runs" after `2d1c70c2`; 03b item 3 and 04b item 3 (stop completion `UNKNOWN` without X15's record); 03b item 1 (polling needs the read acceptance) | 03b item 5: the declared stop carries the caller's fallback deadline and preparation refuses without it; the application invokes it under the shared rule, which admission, TX or acknowledgement never retire. Consequence written there: until X15's record and the read acceptance exist, no stop effect is confirmed, so every run that reaches stop due ends with the declared stop invoked by its fallback deadline; the result names the stop and the deadline, and the application records its invocation. 03b item 6: the same for homing, with the early interruption behind a pending upload (capped only at the latest interruption time) named as a case. 04b items 5 and 6 cite both; the 03b and 04b intros list the fallback deadline among the shared rules. Tests in 03b Deliver, consumed by 04b: a declared stop without a fallback deadline (refused at preparation) and service-level fallback tests through the example application with a recording declared-stop hook and a fake clock (urgent stop not admitted, not dispatched in its window under queue pressure, acknowledged with no confirming observation, early homing interruption with an upload pending), each asserting the invocation by the deadline, no cancellation by admission, TX or acknowledgement, and `needsStop` with motion unknown kept. 07b physical items 4 and 6 put the deadline in the envelope; README S3 bullet |
| F2 TLO actual position | Accepted | Old 03b lines 46-55, 04b lines 24-33, 07a T5 and K5; TLCEN p3 extract (encoder row: TLC "1000 lines", TLO "None"); LX review lines 300-301 ("TLO reported position is not measured feedback"); CL257 p4 and p10 (17- and 23-bit encoders) and ABZ57 p4 as the README cites them; `Actions.h` `EnableEvidence` (the C path compares re-uploaded reviewed values and never `607A` with `6064`) | 03b item 2: the basis record makes the values comparable and does not make `6064` the shaft position; on TLO the attended route also needs the caller's actual-position record for this admission (an independent fixture measurement on the basis record's coordinates, with no motion write since), otherwise the equality is a controller-coordinate screen and the route refuses with MISSING_PREREQUISITE and zero TX, a valid basis record and equal raw values notwithstanding. The same applies on TLC, and in 04b item 2 on CL2, unless the basis record shows that `6064` reports the encoder, which no TL-C or CL2 page states: the gap is the same wherever that is unstated, and the extension is one clause. 07a T5 and K5: production route only with these records, otherwise the bounded experiment or NOT RUN. Tests in 03b and 04b Deliver: equal raw values with a valid basis record and no actual-position record (TLO), and a basis record without the encoder statement (TLC, CL2), each MISSING_PREREQUISITE with zero TX |
| F3 queued targeted NMT | Accepted | Old 03a lines 90-97 and 103-107, 06 lines 18-21, execution contract route table; round 4's F5 change ("no targeted NMT command reaches the owner, so no hand-off case remains"), which held only for commands admitted after the declaration; canopen_shared 02 S13 (NMT admission rechecked at hand-off; a binding change retires a not-yet-sent command), shared contract text whose landing is not established here; `CanBusOwner.h` `admitNmt` and `cancelNmt` as round 3 read them | 03a item 6: the population-change check also refuses a TL-C or CL2 declaration or rebinding on a node holding a pending targeted NMT request admitted under its earlier declaration or binding, until terminal or cancelled (a handed-off frame reported possibly sent, recorded with the declaration); targeted NMT to other nodes untouched. Where S13's hand-off recheck has landed it retires the command on a rebinding and the application consumes that terminal; the application check stays for a declaration, which the owner does not see. Tests: a targeted command admitted under the CL86-C binding with hand-off held, then the declaration and separately a rebinding (refused, no later NMT TX to that node), then cancellation and the declaration (admitted, zero NMT TX), with another node's targeted command handed off unchanged. 03a acceptance, a note after the execution contract's route table, 06 items 2 and 6; 04a item 1 applies 03a item 6 with the same tests. Round 4's wording above is superseded by this row |
| F4 TL-C homing signals | Accepted | Old 03b lines 106-110, 04b lines 66-69, 07a T8; TLCEN p7 extract (`2400:01` "DI Terminal Active Level", DI0..DI2 defaults 1, 2, 3), p8 (DI command table: 1 homing signal, 2 positive limit, 3 negative limit), p12..13 (17 and 18 run to the negative and positive limit switch, 24 and 29 to the home switch, start directions positive for 18 and 24, negative for 17 and 29); CL257 p10 codes as 04a item 8 records them; AGENTS external I/O rule | 03b item 6 "Fixture signals", defined once: beside the X17 record, each method needs the caller's fixture signal record for this binding, model and fixture (from 07a's wiring record): the input wired to each signal the method uses and the polarity it was reviewed with; before setup or trigger one fresh read of `2400:01..06` and `2101` in the current node generation must match it and any starting condition the X17 record states; a missing record, one for another binding, model or fixture, a signal on another input or none, another polarity or an unmet starting condition refuses with MISSING_PREREQUISITE and zero TX. 04b item 6 checks its switch and start-direction prerequisites against the same record and a fresh `2400:01..08` and `2101` read. Tests in 03b and 04b Deliver (correct assignment bytes with no record, another binding's record, another input, another polarity, an unmet starting condition). 07a T8 and K7, 07b physical item 6 |
| F5 re-declaration to a legacy model | Accepted | Old 06 lines 18-22; canopen_shared 02 S3 (explicit no shape set before first traffic and kept across rebind; Lichuan keeps DEC-2) and "Timed runs" (a run's stop path on an unresolved route is the binding's opted-in shape or a declared stop); `CanBusOwner.h` `setSdoReuse` (default `SdoReuse()`; a change "does not reopen, re-quarantine or cancel anything") and the `sdoReuse` copy getter | 06 item 2 defines both directions in the owner's service context. A re-declaration is refused while work is admitted (a pending targeted NMT command included), a result is retained, the route is unresolved or an owner resync step is pending. To a TL-C or CL2 model: `NONE` and the explicit no shape, as before. To CL86-C or OL86-C: the new-model attestation cleared, the owner default `SdoReuse()` of DEC-1 and S3's Lichuan profile default (DEC-2's shape) restored and both read back; until both match the application admits no work there, so no CL86-C run counts on a DEC-2 stop path the binding lacks. 03a item 2's landing of the explicit no shape also lands the return to the profile default. Tests in 06 item 6 and Deliver: both directions on a reused channel, refused with an unresolved previous transfer, then the effective reuse policy and shape (CL86-C best-effort sends `000B`, TL-C or CL2 returns `INVALID` with zero TX). README S3 bullet |
| F6 prompt 57 between stages 02 and 06 | Accepted | Old 02 lines 86-90, prompt 57 lines 201-203 (update only "if the set has closed out first"), 03b item 12, 04b item 13, 06 item 7, 07b item 1 | 02 item 9: the commit that lands a dependency updates the cells whenever the map exists, to the owning stage's disposition, GATED under X15 for a model whose operation stage has not landed its source-state table; console tests and coverage rows only where those routes and files exist, otherwise 03b, 04b and 06 create them from the map's cells; the stage-02 handoff states the rule. 02 Deliver adds the checker case (the dependency landed before 03b, 04b and 06: re-decided GATED cells pass with no console test or coverage row). 03b item 12 and 07b item 1 follow. Prompt 57's sentence needs the matching change (lead decision 1) |
| F7 TL-C-only ordinary reads | Accepted | Old execution contract line 109, 03a lines 95-97, 04a lines 81-82 and 113-116; execution contract "Evidence and supported behavior" (a model without a row refuses before TX); LX review line 193 (CL2 documents only `2401`; `2402..2407` undocumented) | Execution contract: the route table is an identity filter applied after model and object eligibility, TL-C's raw bitrate candidates and error history refused on CL2 under every outcome, CL2's `1018` option the one undocumented read; the ordinary-reads row names reads "the declared model has rows for" and TL-C bitrate candidates. 03a item 6 states the filter and its test asks for frames of the reads "the model admits". 04a item 1 states it; 04a Deliver pairs the zero-frame CL2 request with 03a's TL-C test under the same declared-model match |

### Unresolved question

1. **Answered by the pages, now written in 03b item 2, 04b item 2 and 07a T5.** TLCEN p11 (TL57ZH p10) and
   CL257 p12 only pair each controlword with low bits (6040=0 with 000, 6 with 001, 7 with 011, 15 with 111)
   and document no other starting state (extract pages, which round 4 rendered). 03b item 2's table therefore
   admits one starting row, low bits 000 with bit3 and bit14 clear in the gate's mode, first word `0x0006`,
   then `0x0007` and `0x000F` each after the previous acknowledgement and a fresh report with its paired low
   bits; low bits 111 are reported as already enabled with nothing sent; 001, 011 and other patterns are GATED
   under X15; alarm or running refuses; another mode or a stale, missing or other-model report refuses
   before TX. After an acknowledged `0x000F`, a fresh low-bits-111 report is predecessor 15; any other fresh
   report admits no stop row, so nothing further is sent, no stop is prepared, the result keeps the enable
   state unknown and the attendant uses the power cutoff (T5's fourth branch, which K5 takes). 04b item 2
   applies the table with 04a item 4's bracketed acquisition and `2100` bit2 for bit14.

### Reviewer coverage gaps

- Original pages: this application read the PyMuPDF extracts of TLCEN PDF pages 3, 7, 8, 11, 12 and 13 and
  CL257 PDF pages 4, 10 and 12; no page was rendered as an image this round. Other device claims stay as
  earlier rounds checked them.
- Code: `Actions.h` `EnableGate` and `EnableEvidence`, `CanBusOwner.h` `setSdoReuse` and `sdoReuse`; whether
  S13's hand-off recheck has landed in code was not established; builds and hardware are unchanged.

### Decisions for the lead or owner

1. Prompt 57 (F6), outside this directory. In its "Verify and exit" text, replace the sentence that begins "If
   the Lichuan C extensions set has closed out first" with this text, keeping prompt 57's existing link on
   "Lichuan C extensions":

   ```text
   If the Lichuan C extensions set's stage 02 disposition map exists, this
   landing commit also updates its release and mode-select cells for TL-C and CL2 to the dispositions of its
   stages 03b item 12 and 04b item 13 (GATED under X15 for a model whose operation stage has not landed), and
   updates their console tests and coverage rows only where those routes and files already exist (its stage
   02 item 9), so no cell stays NOT_IMPLEMENTED on a landed dependency.
   ```
2. F1 consequence: until X15's stop record and the read acceptance exist for a model, every TL-C or CL2 finite
   run that reaches stop due, and every interrupted home, ends with the declared independent stop (enable,
   STO or drive-power path) invoked by its fallback deadline. Confirm.
3. F2: the actual-position record is a new MISSING_PREREQUISITE input (proposal), extended to TLC and CL2
   unless the basis record shows that `6064` reports the encoder. On an open-loop TLO the controller's own
   coordinate decides commanded motion; restating the prerequisite in controller coordinates, with the
   unexpected-motion bound kept, would be a separate reviewed change.
4. F5: canopen_shared 02 S3 defines the explicit no shape but no return to the profile default. 03a item 2
   lands the return with the no shape; if S2 lands S3 first without it, adding it is a shared-contract change,
   and the canopen_shared README S3 row could name it.
5. F3: the application's declaration check stays after S13's hand-off recheck lands, because the owner does
   not see a model declaration. Confirm.
6. Proposals to confirm: the fixture signal record (F4) and the already-enabled report answered with no TX in
   the enable source-state table.

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this
directory. Nothing was compiled; no hardware or port was used; nothing was committed.

### Round 5 lead decisions

1. Prompt 57's handoff sentence now covers a landing whenever this set's stage 02 map exists (F6).
2. F1 consequence confirmed: until X15 and read acceptance give TL-C and CL2 stop evidence, a run reaching
   stop due and an interrupted home end with the declared independent stop invoked at its fallback
   deadline, because the urgent stop's effect cannot be confirmed. This relaxes model by model as that
   evidence lands.
3. F5: canopen_shared 02 S3 now defines the way back from the explicit no shape (restore the profile
   default after reconciling the route and previous work, before first traffic, with a readback); 03a's
   path consumes it whichever commit lands S3 first.

## Round 6: audit `aud-20261119-205720-581d91`, 19 November 2026

Each finding of the [sixth report](audit.md) was checked against this set's files at `08040865` (unchanged
from `6fd33ca0` and at `d2be8211`), the [LX review](../../reference/lichuan_c_extensions/review.md), the
extract pages, ledger rows and code named below, and the shared change made after the audit in `d2be8211`,
recorded in [canopen_shared audit_application](../canopen_shared/audit_application.md) theme 19: in
canopen_shared 02 "Timed runs" a fallback deadline already elapsed at dispatch of the first possibly
starting write refuses that write; when service resumes after the deadline with the stop still owed, the
application invokes the declared stop at once and the result reports the missed deadline and unknown
motion; an outstanding obligation blocks re-declaration or rebinding of that node; and a stop path means a
stop usable from every reachable state of the run. F1 and F2 are applied by citing that rule, with the
family tests added. Following the user's direction of 2026-11-19, every high finding was verified and fixed,
and the two medium ones were cheap and clearly right. Only files in this directory changed; the shared
contract, prompt 57, the LX pack, the ledger, code and tests were not edited. No hardware, port or build was
used.

Dispositions: 7 accepted, none accepted in part, rejected or deferred.

| Finding | Disposition | Evidence checked | Change |
| --- | --- | --- | --- |
| F1 stop obligation blocks re-declaration | Accepted, by citation | Old 03b lines 36-38 (an acknowledgement-only end keeps `needsStop` and its stop due time) and 06 lines 18-22 (the re-declaration refusals name admitted work, a retained result, an unresolved route and resync steps, not an owed stop); canopen_shared 02 S3 (restore the default only with no pending stop) and "Timed runs" after `d2be8211` (an outstanding obligation blocks re-declaration or rebinding) | 03b item 5: the application keeps the stop obligation (`needsStop` with its stop due time and fallback deadline) independently of the run's result; releasing a retained result does not retire it, and while it is owed a rebinding or re-declaration of the node is refused, citing the shared rule. 06 item 2 adds an outstanding stop obligation to the re-declaration refusals, which also covers the legacy-shape restore it performs. Tests: 03b Deliver (an acknowledgement-only run released before stop due, then a rebinding: refused, owner settings unchanged, the urgent stop and the declared stop still serviced), 06 item 6 and Deliver (the same for a re-declaration), 04b Deliver consumes them |
| F2 fallback deadline validity and late service | Accepted, by citation | Old 03b lines 129-137 and 287-289 (homing's "a service cycle resuming after it" named no expected result); canopen_shared 02 "Timed runs" after `d2be8211`; 07b physical items 4 and 6 (the fallback deadline is in the caller's approved envelope) | 03b item 5 applies the shared validity and late-service clauses (elapsed at dispatch of the first possibly starting write refuses that write; service resuming late with the stop owed invokes the declared stop at once and reports the miss and unknown motion, no late start) and states the premise: meeting the deadline rests on the application's service cadence or an actuation path independent of it, which the library assumes neither way. 04b item 5 cites both. Tests in 03b Deliver: an elapsed fallback deadline at dispatch (that write and every later one unsent), a service-level resumption after the fallback deadline (hook invoked at once, miss reported), and the homing resumption case split into after the latest interruption time (no further home work, HALT requested at once where admissible) and after the fallback deadline. No ordering rule between the fallback deadline and stop due is added: an earlier fallback deadline only invokes the declared stop earlier, and the time limits stay caller- or envelope-supplied (07b) |
| F3 declaration interlock after S13 | Accepted | Old 03a lines 103-124 (the application rule, including the population-change check, retired when S13's full gate lands); canopen_shared 02 S13 (approval revoked by remap, rebind, NMT reset or boot-up, a configuration generation change and owner invalidation; NMT admission rechecked at hand-off on a binding change); 06 item 2 (a declaration replaces the application's `ModelBinding` in the owner's service context, no rebind) | 03a item 6 separates the two rules: the interface-wide deny (no `armSync`, `admitSync` or node-0 NMT) retires when S13's full gate lands with this set's unknown effects; the declaration interlock stays, because a model-only declaration is none of S13's revocation triggers, and retires only in a reviewed change whose test shows a same-channel declaration updating S13's evaluated population and revoking earlier approvals atomically with owner admission and hand-off (approved schedule on an all-CL86-C interface, held one-shot SYNC and held targeted and broadcast NMT, then a model-only declaration: no stale SYNC or NMT after a successful change, handed-off frames keep their possibly-sent disposition). 03a acceptance, 06 item 6 and the README S13 bullet follow |
| F4 halt-state derivation at stop dispatch | Accepted | Old 03b lines 82-94 and 269-271, 04b item 3; canopen_shared 02 "State transitions" (halt clear only after an acknowledged bit-8-clear word with no later possibly sent bit-8-set word, or a controlword readback where the ledger shows `6040h` returns the received value; a declared external writer makes it unqualified) and "Ownership premise"; 03a item 4 (TL-C status bits 3, 8 homing complete, 14, 15; no halt bit) and 04a item 4 (CL2 bits 8, 14 and 15 without meaning) | 03b item 3: the halt state at dispatch follows that derivation; a declared external writer makes it unknown; the status bits never decide it; no TL-C source states that a `6040` upload returns the received word, so the readback alternative stays unused until a reviewed row shows it. 04b item 3 applies it for CL2. Tests in 03b Deliver, consumed by 04b: an acknowledged `000F` followed by a possibly sent `010F`, and a declared external writer after an acknowledged bit-8-clear word, each on an admissible route with a fresh low-bits-111 report, emitting `010F` for HALT and `010B` for the quick stop and recording the halt state unknown with its reason. The selected unknown-halt word is unchanged |
| F5 position moves after a possibly sent trigger | Accepted | Old 03b lines 112-118 and 126-128, 04b line 58; canopen_shared 02 "Timed runs" (a run the host ends; a stop path usable from every reachable state, `d2be8211`); ADD_DRIVE.md line 25 (deadline service of finite onboard trajectories); AGENTS no-replay rule; the engineering direction of 2026-11-17 (settable guards, caller responsibility) as the lead's rounds apply it | 03b item 4 "Interruption choice", defined once: preparation takes the caller's choice before any changing TX. If the envelope requires interruption, the caller supplies a latest interruption time and the move is a host-ended run under the shared timed-run rule with item 5's stop path (stop budget before the one deadline, transfers capped at stop due, an application-declared independent stop with its fallback deadline, all before the trigger), otherwise MISSING_PREREQUISITE with zero TX; a move confirmed complete before that time owes no stop. Otherwise the result states the onboard-completion premise the caller accepted and, after a lost trigger reply, keeps motion and completion unknown, sends nothing further and names no usable stop path; no trigger replay, no completion from elapsed time. 04b item 4 consumes it (CL2 has no S3 shape, so the declared stop). Tests in 03b Deliver: a lost trigger reply under `NONE` in both choices, and an interruptible move without a latest interruption time or declared stop (refused, zero TX). 03b and 04b intros, README stage table (item 4 among the shared rules) and 07b physical item 4 (the choice in the envelope) follow |
| F6 CL2 `6098` write policy | Accepted | Old 04a lines 99-100 and 04b lines 67-84; LX review LX-D33 (`6098` I8 p19 against U8 p13); CL257 PDF p13 extract ("6098h 00 回零模式 模式介绍见附录一 UNSIGNED8 RW") and p19 ("6098 00 Homing method (RW) I8"), the same pair on p13 and p19 of the CL286, ABZ57 and ABZ86 extracts; the appendix method list 1..14, 17..30, 33/34 (p31 "6098=33/34"), 35 in all four; the C family's C22 review in `lichuan_objects.json` ("positive codes 17/18/24/29 are wire-identical in U8/I8", write policy `qualification_required`); `Homing.cpp` `INDEXES` (`6098`, `6099:01/02`, `609A`, `607C`) and its mode-6 precondition (no mode write); canopen_shared 02 "Write preflight" | 04a item 11: `6098:00` gets an explicit CL2 write-policy row with C22's narrow disposition: both claims are one byte and every listed CL2 method is positive and below 0x80, so its wire byte is the same; only those values are admitted, the conflict stays recorded, and each manual's rendered p13 and p19 are reopened. 04b item 6: write preflight before any homing setup download over every write the home may issue (`6098`, `6099:01/02`, `609A`, `607C` and its `6040` words) against reviewed CL2 write-policy rows; a missing or unresolved one blocks the whole home with nothing sent; method or fixture evidence or a fake server's acceptance never stands in. Test in 04b Deliver: a missing or conflicting `6098` row with a valid method record sends no homing setup download at all |
| F7 the stage 06 loader for changing first-contact questions | Accepted | Old 07a lines 10-11 and 51-53, 06 lines 33-35 (static configuration, the one loading path for a MEASURED record), README stage table row 07a | 07a: changing questions run after 03b or 04b and 06 item 2's static configuration, the one path that loads a MEASURED record (and, after the unresolved question below, the caller records and declared-stop hook); before stage 06 lands they are NOT RUN or assigned experiments, read-only capture still runs and its captures (T1, K2 included) stay evidence awaiting loading. T5 and K5 name 06; 07a Deliver and the README 07a row follow. The loader is not moved earlier, which would add scope to 03b and 04b |

### Unresolved question

1. **Answered by the code, now written in 06 item 2.** No such path exists today. The shared console creates
   no qualification: `examples/common/ProbeConsole.cpp` lists `home qualification` and `velocity
   qualification` as "No console-created qualification" rows ("CLI cannot attest a measured record"). Evidence
   reaches motion only through `ProbeMotion`'s `setMovePrerequisites`, `setVelocityPrerequisites`,
   `setHomePrerequisites` and `setEnablePrerequisites` (`examples/common/ProbeMotion.h`), called by application
   code: the CL86-C-only `examples/s2_bench/BenchConsole.cpp` builds move and velocity evidence from its own
   arming plan. No code has an application-declared independent stop (the "independent stop" of
   `Velocity.h` and `Homing.h` is the composed CANopen urgent stop). 06 item 2 now names the path: the same
   static configuration that loads identity records also loads the fixture-scoped records 03b and 04b take
   (model evidence records, basis, fixture signal and Z fixture records, the X14 rating), each with its
   provenance id and checked at load, and binds the declared independent stop as an application hook on the
   fixture's own enable, STO or power path; request parameters (fallback deadline, latest interruption time,
   travel bound, stop budget, save acknowledgement, move interruption choice) are verb arguments; a
   per-admission actual-position record comes only from a measurement source named in the configuration,
   never from console text; a missing record, source or hook prints MISSING_PREREQUISITE with zero TX. 06
   Deliver adds parity runs with configured records and a recording hook and that refusal.

### Reviewer coverage gaps

- Original pages: this application read the PyMuPDF extracts of CL257, CL286, ABZ57 and ABZ86 PDF pages 13,
  19 and the homing appendix; no page was rendered as an image this round, and 04a item 11 asks the
  implementer to reopen each rendered page. Other device claims stay as earlier rounds checked them.
- Code: `ProbeConsole.cpp` qualification rows, `ProbeMotion.h` prerequisite setters and their callers in
  `BenchConsole.cpp`, `Homing.cpp` `INDEXES` and mode precondition, and the ledger's `6098:00` entry and
  conflict C22. Whether S13's hand-off recheck or full gate has landed in code was not established; builds
  and hardware are unchanged.

### Decisions for the lead or owner

1. F5: the onboard-completion choice is a caller-accepted premise in the spirit of the 2026-11-17 direction
   (settable guards, caller responsibility); a move without the interruption requirement can leave motion
   unknown with no usable stop path after a lost trigger reply. Confirm, or make the interruption
   requirement the default.
2. F3: the declaration interlock outlives S13's full gate until a reviewed change proves a model-only
   declaration revokes S13 approvals atomically. If S13's landing should instead expose the application's
   model declaration to the owner (a shared S13 change), the canopen_shared README S13 row could say so.
3. Unresolved question: the static-configuration path for caller records and the declared-stop hook, and the
   actual-position record coming only from a configured measurement source, are proposals for stage 06.
   Confirm.
4. F6: `6098:00` for CL2 adopts the C family's C22 narrow disposition (positive listed methods only); confirm
   before stage 02 or 04a writes the row.

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this
directory. Nothing was compiled; no hardware or port was used; nothing was committed.

### Round 6 lead decisions

1. F5: onboard completion stays the default for a finite position move (as for the CL86-C move, which needs
   no declared stop); interruption is the caller's opt-in with a latest interruption time and a declared stop
   before the trigger. The result states which was chosen.
2. F3: the declaration interlock lives until a test proves a model-only declaration revokes S13 approvals
   atomically with admission and hand-off; no shared S13 change now.
3. Stage 06's static records and stop-hook path is the example application's input path: confirmed.
4. CL2 `6098` narrow U8/I8 disposition (checked in the four manuals' extracts): confirmed; original-page
   confirmation is part of stage 04a's own evidence step.
