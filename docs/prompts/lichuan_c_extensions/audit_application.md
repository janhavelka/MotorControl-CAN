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
