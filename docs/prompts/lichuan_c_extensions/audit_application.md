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
