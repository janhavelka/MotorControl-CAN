# Application of the MOONS' CANopen promptset audit, 19 November 2026

Each finding of [audit `aud-20261119-175226-dd78d8`](audit.md) was checked before anything
changed: against this set's files (byte-identical to the audited ones), the pack
[review](../../reference/moons_canopen/review.md), the original pages through the page-marked
UM21 and AMP10 extracts, the three EDS files parsed section by section, and the shared
contract as revised in `1043608b` (README seams S1-S14, 02 "Operation admission" and
"First-contact questions", [round 2](../canopen_shared/audit_application.md#round-2-family-audits-19-november-2026)).
Prompt 57's identity policy was read at `0ab4e181`. Following the user's direction of
2026-11-19, the fixes stay with the findings. Where a finding is one of round 2's shared
rules, the family stage now cites the shared rule instead of restating it. Only this
directory's files changed; no pack, shared, main-sequence or source file was edited.
MotorControl-RS was not consulted: no finding concerns a common motion name.

Dispositions: 11 accepted, 3 accepted in part (F3, F7, F9), none rejected or deferred.

## Findings

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 | Accepted | Old 09 item 9 and acceptance (one fresh non -2 read admitted SYNC); shared 02 S13 bullet (bus population, live NMT and PDO state, known-harmful effects, revocation during a schedule); UM21 p51 (a synchronous RPDO "shall actuate the received data with the reception of the next SYNC"), p53 (TPDO types), p70 (watchdog fed by RPDOs or SYNC), p14 (SYNC served in Pre-operational and Operational) | 09 item 9 consumes the shared S13 bullet and supplies the MOONS' entry: per node mode, watchdog (0x2060 sub 1/4) and PDO communication and map reads; mode -2 always refused; SYNC-fed watchdog refused unless stage 11's record names the schedule; a valid synchronous RPDO refused without an S14 producer; synchronous TPDOs admitted after readback; approval revoked before a 0x6060, 0x2060, 0x1005 or PDO write is admitted. 09 acceptance lists the shared classes with MOONS' nodes (target and non-target), including revocation during a running schedule |
| F2 | Accepted | EDS-STF and EDS-SSDC sections `[6087]` (SSDC UINT32 rww), `[2020]` (STF UINT16, SSDC UINT8), `[2060sub1..5]` (STF UINT16; SSDC UINT8, sub 2 ro), `[1400sub1/2]` (rw, sub 2 UINT8), `[1800sub1]`, `[1A00sub0]`; UM21 p33 (0x6087 INT8 WO), p62 (UINT32 RW), p38 and p70 (0x2060 UINT8 subs 1, 2, 4; UINT16 sub 3, 5), p50 (0x1400 subs RO, sub 2 UNSIGNED32), p53-p54 (TPDO objects RW), p65 and p68 (0x2020 UINT16 and UINT8), p31 and p64 (0x60FF UINT32 and INT32); shared 02 "Operation admission" write preflight | Execution contract: every operation follows the shared write preflight, one deadline and source-state table. 02 item 4: conflicting access withheld like width; signedness-only conflicts (0x60FF, 0x609A) admit only the common range 0 to 2^31-1; 0x2100 raw words unaffected; per-product list of withheld writes at this revision. 06: torque refused whole until MO-D04's 0x6087 encoding resolves; a negative 0x60FF refused. 09 item 7: RPDO remap refused, TPDO remap admitted. 11: the 0x2020 change refused on both products; watchdog arm and re-arm refused (sub 2 is in both products' withheld list); each with zero-changing-TX tests |
| F3 | Accepted in part | UM21 p38 ("0x2-0x14 ... 1-12 segment") and p70 ("0x2-0x0Eh"); review section 7 (no default host-loss reaction); AGENTS host-heartbeat rule; S14 absent (shared README); prompt 51 producer under S13 | Accepted: 10 item 8 and 11 item 6 refuse every action of 2 and above until the action-to-segment mapping is resolved **and** a program qualification covers the segment; 06 item 5 publishes "host-loss reaction: none established; motion may continue"; 11 requires an owner-admitted feed (S13 SYNC naming the watchdog, or S14 RPDO, which refuses until landed) and reports "armed, action N, not observed"; tests for an unresolved mapping, an RPDO trigger without S14 and a SYNC feed not named by the approval. **Not adopted as written:** the applicable mode is reported, not required: UM21 p70 calls the watchdog "a safety feature in Profile Velocity mode" without saying it fails elsewhere, arming adds a stop rather than motion, and the user's direction keeps host-loss safety with the application (2026-11-12) and prefers optional guards (2026-11-17). Outside mode 3 the result says the reaction is outside the documented applicability |
| F4 | Accepted | Shared 02 S1 (`LICHUAN_C = 1`) and S2 migration (refuse 41; tests keep an unchanged migrated Lichuan binding); old 02b item 2 (refused 41 and 1) | 02b item 2: value 1 stays valid with unchanged traffic; 41, zero, unknown, another family's value and protocol mismatch refused; literal-1 callers move to the enum; S3's default deny lands in the same S2 commit (shared 02 S3 bullet) |
| F5 | Accepted | Shared round 2 note (MOONS' F5 stays refusal-only; a positive SYNC Q start needs its own shared-contract change); old 10 item 6, 10 acceptance, 14a Q11 | 10 status and item 6: refusal-only, no 0x6060 = -2 write in any path, a node reading -2 keeps S13 refusing, owning a schedule is no exception; the positive route named as a separate shared-contract change. README stage row, rationale paragraph and seam paragraph; 14a Q11 no longer runs mode -2 |
| F6 | Accepted | UM21 p21 ("After releasing the halt bit the processing of the actual set-point is continued"; bit 12 note); p16 transition 4 ("all internal set-points cleared"); p55 (0x6040 write-only); old 05 item 7 (start word 0x000F/0x004F clears bit 8) | 04 item 8: a per-binding controlword record for the server session with a "set-point may be pending" state, cleared by a confirmed enable transition 4 (documented, unqualified until 14a) or an observed boot-up, unknown after a lost reply, rebinding, a new session or a host restart; halt release is its own operation. 05 item 7: idle admission separate from buffer space; a pending or unknown record refuses with nothing sent. Tests in 04 and 05 (halted pending move with bit 12 low, after rebinding, with the record lost) |
| F7 | Accepted in part | Review section 4 (series, version and vendor ID together; 0x2100 differs by layout); UM21 p69 (0x2050 default "--": the values "STF-"/"SSDC" are EDS defaults only); EDS `[100A]` "300F"/"300E"; old execution contract (firmware gates operations) against old 03 item 3 (gates claims); prompt 57 identity policy and its 2026-11-12 user requirement ("Motion is locked to the exact measured firmware - we dont need"), `0ab4e181` (CONTRADICTED refuses changing requests; firmware never produces it); `SdoTransfer.h` | Accepted: 03 item 2 publishes an admission table (bootstrap, UM21 layout and series, exact model declared only, firmware) with prompt 57's CONFIRMED, CONTRADICTED and UNCONFIRMED outcomes, stale observations without an outcome, a TSM's 0x2050 not-exist abort as CONTRADICTED; UNCONFIRMED also refuses changing operations because 0x2050 is MOONS' only layout discriminator. 13 items 3 and 7: "UNK", "2.12" or an arbitrary abort is no model evidence; model-specific Part B operations need the manual and model identification evidence; Part A admits no mode. Host tests use labelled EDS values or explicitly synthetic qualifications. **Resolved the other way:** the execution contract's firmware gate becomes "reported, not a lock" (unqualified firmware refuses nothing and is carried in every terminal result; hardware-qualified claims still need the qualification; only recorded evidence of a pre-310A/310B line refuses), matching prompt 57 and the user's direction |
| F8 | Accepted | UM21 p16 transition 12 and p58 (codes 1 and 2 end in Switch on disabled; no shaft statement); UM21 p35 and p89 (Q halt word 0x011F only); AMP10 p48 (halt immediately, restart from the beginning, 2010 layout); review lines 305-308 | 04 item 7: word sent, state observed and mechanical effect (not observed) published separately; 04 item 8 and 10 item 3: the AMP10 Q halt and restart semantics are not UM21 claims; "halt commanded" with the restart point unknown until 14a Q11. Tests in 04 and 10 that acknowledged halts and quick stops publish no holding, release or restart point |
| F9 | Accepted in part | UM21 p30 (rendered extract: method 35 "does not require the drive device to be in operational enabled state"; method 37 the same, with 0x6064 = 0x607C); p23 and AMP10 p28/p43 direction wording (MO-D07); review section 7 (no documented limit reaction); prompt 77 limit admission (direction guard on by default, settable per axis, named in the result) | 07 item 3: moving methods need a caller direction and approach qualification bound to binding, configuration generation and fixture evidence, invalidated by direction, polarity or input-function changes; 07 item 4: wiring does not prove approach; 07 item 6: a caller-reviewed stop and travel plan, a host deadline bounds nothing physical; 07 item 9: 35 and 37 both need no Operation enabled and get their own table, refused where the pages leave the sequence open; tests for unknown and stale direction. **Adapted:** where prompt 77's direction guard has landed, the refusal is that guard (on by default, settable per axis, an admitted result names it as off), the repository's existing policy for the same hazard |
| F10 | Accepted | Shared 02 "First-contact questions"; `docs/device_conflicts.md` "Conditional experiment prerequisites" (item 1: no choice between competing write recipes); 05 item 10, 11 item 1, 02b item 7 | 14a intro cites the shared rule; each question names a path. Q7: vendor clarification or a reviewed experiment with the documented two-set-point sequence. Q9: stage 11's route once its writes and feed exist, vendor clarification, or an SSDC-C-only experiment where the arming widths agree; none on STF-C. Q12: store through stage 11; the node-ID part only after vendor clarification of width and store sub-index, then an experiment with snapshot and restoration boundary. Q13: vendor clarification or a passive listen-only experiment with no host TX. Otherwise NOT RUN. Run order and rule 6 updated |
| F11 | Accepted | Old 07 status (requires 03-05), 08 item 1 ("Stages 07, 10 and 11 reuse it"), 10 status (04 and 09 only, same gap) | Option two: 07 is self-contained (`HomeContext` writes its parameters as confirmed steps like stage 05; input functions through stage 03's config read). 08 item 1: the context lands with the first block; stages 10 and 11 reuse it. 10 status and the README row now require 08's settings context |
| F12 | Accepted | Shared 02 "One deadline" bullet; old 05 item 7 and acceptance; 06 item 4 (distinct dispatch and wire deadlines); 07 item 6 | 05 item 7: one absolute deadline captured at preparation bounds every write and poll, expiry checked before each changing step, the trigger included; 05 and 06 acceptance test cumulative delay with no trigger TX; 06 and 07 bound their inner deadlines by the operation deadline |
| F13 | Accepted | `SdoTransfer.h` (`size` 1..32, `SdoUploadSize::SERVER_INDICATED` capacity, `allowSegmentedUpload`); `Sdo.h` `SEGMENTED_UPLOAD` ("never an expedited value or completion"); UM21 p11 | 03 item 4: fixed documented capacities within the 32-byte limit chosen at stage 03; complete, incomplete, oversized, server-abort and no-answer field states; only complete fields count; a segmented initiate is support evidence, not a value; other fields unchanged. 03 acceptance lists exact fit, one byte over, no indicated size, interrupted segments |
| F14 | Accepted | Shared 01 S7 bullet ("that registration is itself an S7 edit, so a stage that registers a checker before its first header claims S7 first"); shared README S7 row | 01 item 7: claim or extend S7 before registering the checkers, recording what stage 02's first-header commit still owes; 01 Deliver; 02 item 8 extends that claim; README seam paragraph |

The CANopen principles pack wording is fixed in the README (committed at `e02ab04f`, AKD
manual remote-only, secondary evidence).

## Unresolved questions

1. **Answered by the files.** No identity-dependent MOONS' preparation exists at 02b: the
   probe is a bootstrap read. 02b item 3 now tests what 02b has (the probe sends only its two
   reads and publishes no product with the correct label on a Lichuan server, a missing,
   stale or colliding identity; selection and cached queries send nothing) and moves the
   dependent-refusal cases to the first dependent operations: stage 03's layout-typed reads
   and every changing operation from stage 04.
2. **Answered by the shared contract.** None. The owner admits no PDO data frame
   (`BusRequest` carries an `SdoTransfer`; S14 is not landed). Q9's RPDO feed waits for S14;
   its SYNC feed needs prompt 51's producer admitted by S13 with the watchdog named. Q9 says
   so and is otherwise NOT RUN.
3. **Answered here.** Stage 05 now owns the common angle-move row: it uses prompt 34's core
   reference, scale and angle-path policy through stage 05's scale qualification naming
   native units per revolution, and is `UNSUPPORTED` with the MO-D03 reason without it.
4. **Answered by the repository.** `e02ab04f` committed `docs/reference/canopen_principles/`;
   the README now links it normally.

The coverage gaps (secondary evidence, truncated index, absent fixture) are the executing
stages' intake work: stage 01 reopens every cited page and checks the pack hashes in the
working tree, INDEX and HEAD before MO-D31 to MO-D33 and the 0x1011 extension enter the pack.

## Observed while checking, not changed

- UM21 p89 shows "Change Target Velocity to 80000 step/s" while profile velocity runs; stage
  06 item 6 says UM21 documents no change during motion. Not a finding; stage 06 should
  record the example when it runs (the change stays unsupported until prompt 52's contract).
- 0x1010 is RO in UM21 p40's list and RW per sub-index on p45 and in the EDS (MO-D09). The
  stage 02 access review decides it; if it stays withheld, stage 11's store is refused too.

## Decisions for the lead or owner

1. Firmware stays reported, not a lock (F7), matching prompt 57; confirm for MOONS'.
2. UNCONFIRMED layout refuses MOONS' changing operations, stricter than prompt 57 (F7).
3. The common-range reading of a signedness-only conflict (F2) keeps velocity and homing
   usable but refuses negative 0x60FF: MOONS' velocity is forward-only until MO-D09 is
   resolved for 0x60FF.
4. Profile torque, the node-ID change, watchdog arming and RPDO remapping stay refused until
   MOONS' clarifies 0x6087, 0x2020, 0x2060 and 0x1400 encodings and access (MO-D04, MO-D09)
   and the watchdog action mapping (MO-D18). One vendor question list would unblock them.
5. The watchdog's mode applicability is reported rather than required (F3).

Checks: `python scripts/check_promptset.py` reports no error for this set; `git diff --check`
is clean for this directory. Nothing was compiled; no hardware or port was used.

## Round 2

Audit [`aud-20261119-184315-2fffe2`](audit.md#round-2) was checked finding by finding against this
set's files (byte-identical to the audited ones), the pack [review](../../reference/moons_canopen/review.md),
the page-marked UM21 extract with the rendered UM21 p16, and the three EDS files parsed section by
section. The cross-cutting part was applied once in `a7656efb`
([shared round 3](../canopen_shared/audit_application.md#round-3-cross-cutting-findings-of-the-family-round-2-audits-19-november-2026)):
prompt 57's Identity policy ("Admission by outcome", the stop exemption's scope, NMT never that
stop) and canopen_shared 02 ("Identity and device evidence", the timed-runs bullet of "Operation
admission", the S3 and S13 bullets, "Registration acceptance"). F1, F2 and F4 now consume those
rules by citation and add only MOONS' facts. Only this directory's files changed; no pack, shared,
main-sequence or source file was edited. MotorControl-RS was not consulted: no finding concerns a
common motion name.

Dispositions: 8 accepted, 1 accepted in part (F5), none rejected or deferred.

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 | Accepted | Shared 02 S13 bullet after `a7656efb` (node-0 NMT at the owner's NMT admission, default deny in the S2 commit, per-command NMT effects, revocation before an NMT command that can change an evaluated node's state, the Start tests); UM21 p13 (commands 0x01, 0x02, 0x80, 0x81, 0x82), p14 (state diagram and service table: PDO in Operational only, Stopped serves node and error control only), p51 (synchronous RPDO applies at the next SYNC), p53, p70 (watchdog fed by RPDO or SYNC); prompt 57 (an NMT command is a changing request) | 09 item 1: every NMT command passes the owner's NMT admission under S13, the S2-commit default deny refuses node-0 NMT until item 9 lands, an NMT command to a MOONS' binding is a changing request. 09 item 9: the seam named "SYNC and NMT-broadcast effect admission"; a per-command NMT effect entry (Start harmless only with every RPDO of the node read back invalid; Stop, Enter Pre-operational and both resets unknown, refused for node 0); revocation before an NMT command and on a heartbeat or boot-up reporting an unevaluated state. 09 acceptance: targeted and broadcast Start with a Pre-operational node holding a synchronous RPDO, external state change, node-0 commands with zero NMT TX, also under the default deny. 02b item 2: S13's default deny lands with S2 and S3's. README and execution contract name node-0 NMT under S13 |
| F2 | Accepted | Prompt 57 motion rule and stop exemption (removes only the identity refusal; route, generation and source-state checks stay; a prepared or queued stop is not withdrawn; other pending steps retire); shared 02 "Identity and device evidence" and S3 bullet (shape needs the word's reaction from every reachable state, a set halt bit included); rendered UM21 p16: the command table labels bits 7, 3, 2, 1, 0 = 0 x x 0 x with transitions 7, 9, 10, 12 "Quick stop" (the CiA 402 disable voltage pattern) and has no row with bit 2 clear and bit 1 set, while transition 11's event is "Quick stop command"; UM21 p37 and p56 name controlword bit 2 "Quick stop"; no UM21 example sends 0x000B; UM21 p21 (halt release continues the set-point), p32, p34, p23, p35 (halt words 0x010F, 0x011F), p75, p87 (enable input) | 04 item 9: a stop table per identity outcome. Quick stop 0x000B admitted under every outcome through `admitUrgent` (no ramp or standstill claim under a refusing outcome); halt admitted while the controlword record is known, its word fixed after a lost start or trigger reply (0x010F or 0x011F as documented), refused with an unknown record; disable voltage not exempt. S3 keeps the default deny: the reaction review fails on MO-D34 and on the unstated precedence of halt release in 0x000B; an opted-in shape exists only while its premises hold. "No software stop established" names the application-owned fallback (enable input or power removal). 04 item 8: a lost reply keeps two candidate words. 04 item 7 records MO-D34; 01 item 3 adds MO-D34 (the ID freed by the MO-D24 correction), with count updates in 01, 14 and the README. 03 item 2 and 02b item 3 cite the exemption. Tests in 04 for invalidation during motion and a Lichuan server under the MOONS' label |
| F3 | Accepted | UM21 p19 and p88 (0x0006, 0x0007, 0x000F, then mode 1), p32 (velocity: 0x010F, mode 3, start 0x000F, stop 0x010F), p34 (torque), p35 (Q: 0x010F, start 0x001F, halt 0x011F), p16 (transition 4 clears all internal set-points; transition 16 only for option codes 5-8), p58 (codes 1 and 2 only), p22-p23 (homing starts on a bit 4 edge); shared 02 "State transitions" | 04 item 1: a source-state table from fresh 0x6041 and 0x6061; the final word is 0x010F, not optional, in modes 3, 4, -1 and any undocumented mode, 0x000F only in modes 1 and 6; already enabled sends nothing; Quick stop active, faults and Not ready to switch on refuse; -2 refuses. 04 item 3: a mode change into 3, 4 or -1 while enabled needs halt set in the record (same hazard, cheap). 04 acceptance: the first frame or no TX per row and mode, a present target not run |
| F4 | Accepted | Shared 02 timed-runs bullet after `a7656efb` (Lichuan `FiniteVelocityRequest::durationUs`, `requiredStopBudgetUs`, `needsStop`, phase `STOPPING`); old 06 item 4; UM21 p32 (stop 0x010F) | 06 item 4 consumes the bullet by citation (duration from preparation, stop budget checked at preparation, transfers bounded by the stop due time, separate urgent stop) and names the MOONS' stop row: stage 04's halt 0x010F, composable after a lost start reply, quick stop as the alternative. 06 acceptance adds the shared timed-run cases |
| F5 | Accepted in part | Review section 4 ("must use 0x2050, 0x100A/0x2031 and the vendor ID together"); EDS sections `[100A]` (300F, 300E, 2.12), `[2031]` (no default), `[2050]` (STF-, SSDC; absent in EDS-TSM), `[1018sub2/3]` (0); UM21 p69 (no 0x2050 value); prompt 57 outcome definitions and "Admission by outcome"; shared round 3 theme 2 (lead decision: DS2 F1's refusal of a colliding match rejected, the declaration selects) | Accepted: 03 item 2 lists references per row (vendor ID and 0x2050 "STF-", "SSDC" or the 0x06020000 abort), defines CONFIRMED as uniqueness over the application's candidate set, and makes the manufacturer-area config and state reads layout-typed: bootstrap is the probe, the identity plan and the CiA 301/402 objects, each listed with its reviewed read effect. 03 acceptance: wrong, missing, unmatched and stale identities send exactly the bootstrap reads. 01 item 3 corrects review section 4: the version strings have only EDS defaults matching no firmware row, and firmware is reported, not a lock. **Not adopted:** "no dependent TX" for an indistinguishable reference and a candidate without a reference. Prompt 57's admission by outcome admits both with every matching row named, and a family may refuse more only with recorded evidence; MOONS' has none specific to those cases, and the lead rejected the same refusal for DS2. This supersedes round 1's decision 2: its UNCONFIRMED cases (no answer, another abort, an incomplete or oversized string, another value) are refused by prompt 57 itself now |
| F6 | Accepted | 11 item 6 (feed named by the S13 approval), 09 item 9 revocation before a 0x2060 write; UM21 p38 (arm: sub 1 then sub 2; re-arm), p70 (timeout default 500 ms, fed by RPDO or SYNC) | 11 item 6: a SYNC-fed arm stays refused after the write conflicts resolve, until a reviewed transition defines the approval before the write, revocation, readback, reapproval and a first-feed deadline from measured worst-case service and queue delay; its tests named. 11 acceptance. 14a Q9: the SYNC-fed path needs that transition; otherwise NOT RUN |
| F7 | Accepted | 14a intro (no temporarily opened production refusal); shared 02 "First-contact questions"; `docs/device_conflicts.md` "Conditional experiment prerequisites" (no invented or competing recipes); 04 item 4 (one reviewed release word), item 9 (disable voltage stop kind); 07 items 3 and 9; UM21 p23 (0x001F trigger, which enables, so it cannot be the non-enabled 35/37 trigger) | 14a Q4: the reviewed release word and 0x0000 through stage 04's routes; the other words by vendor clarification or a reviewed isolated experiment, otherwise NOT RUN. 14a Q10: (a) qualified methods through 07; (b) first direction discovery by vendor clarification or a reviewed per-method experiment with snapshot and restoration boundary, never with prompt 77's guard switched off; (c) 35 and 37 by vendor clarification only. 14a evidence-path paragraph updated |
| F8 | Accepted | 08 item 4 and block rows; execution contract ("a documented object without reviewed units, range and effect is read-only"); UM21 p85 (0x2610 masks "the LED flashing"; 0x221A the same), p67 (0x200B bit 0 Motor Enabled, bit 14 Q running); principles CP-D11 (Switched on power-stage meaning differs) | 08 item 4: configuration quiescence (fresh Switch on disabled or Ready to switch on, Motor Enabled and Q running clear) for every 08a and 08c write and 08b's function, brake, filter and analog settings; 0x60FE values need only the output-function check; current writes carry the caller's limits; an unconfirmed write leaves the setting `UNKNOWN`, invalidates at once and inhibits dependent operations until readback; the alarm masks are read-only. 08c row and acceptance updated. Lead decision 4 below then made the quiescence a settable guard and admitted the LED-mask write |
| F9 | Accepted | Shared 01 "Shared code that still assumes Lichuan" (`DiscoveryScan` typed on `LICHUAN_C`, prompt 64 owns the change); 12 status line | 12 item 9: the integrated scan and its test consume prompt 64's artifact and are Blocked until it lands; no parallel scanner; 02b's API probe stays available. 12 Deliver |

### Unresolved questions

1. **Answered by the files.** No positive evidence exists. The abort is negative evidence that any
   MOONS' server without 0x2050 shares (the AMP10 2010 drives, and possibly MDX+, M56S or SV200,
   whose dictionaries are not in the pack). 13 item 3 now says so: it confirms only among the
   declared candidates; with another such candidate declared, the outcome is UNCONFIRMED with the
   declared match (admitted by prompt 57, both named), Part A reads stay labelled inferred, and
   Part B keeps its manual and model-identification gates.
2. **Answered in part.** The promptset does not pre-decide 0x1010; stage 02's access review does
   (MO-D09: RO on UM21 p40, RW per sub-index on p45 and in the EDS). 11 item 7 now separates the
   signature encoding and the preflight refusal while the write is withheld, a production store
   test only once stage 02 admits the sub-index, and durability only through `verifyPersistence`
   after a real restart on a fixture; 11 acceptance names the refusal. 14a Q12 was already
   conditional on the preflight.

The coverage gaps are the executing stages' intake work, as in round 1. For the device claims
used here, the rendered UM21 p16 and the extract pages named above were reopened.

### Lead decisions, 19 November 2026

The four questions this application raised were decided by the lead and applied:

1. **Agreed, kept.** MOONS' follows prompt 57's admission by outcome without a stricter
   UNCONFIRMED rule: a collision or a candidate without a reference admits by the declaration
   (F5). This supersedes round 1's decision 2 and matches the DS2 F1 decision.
2. **MO-D34 confirmed by the lead on the rendered UM21 p16.** 0x000B stays the exempt quick stop
   under a refused identity. The exemption rests on UM21 p37 and p56 (controlword bit 2 named
   "Quick stop") and p16 transition 11 ("Quick stop command"), not on the p16 command table. MO-D34
   stays open for 14a Q8's observed transition and a vendor question; no best-effort S3 shape.
   Recorded in 04 items 7 and 9 and 01 item 3.
3. **Agreed.** Disable voltage 0x0000 is not exempt under a refusing identity outcome (F2).
4. **Changed.** F8's configuration quiescence is a guard, on by default and settable per binding;
   an admitted result names a guard that was off (prompt 77's limit-guard pattern, item 2; the
   owner's 2026-11-17 direction for optional, settable guards and user responsibility). The alarm
   masks 0x2610 and 0x221A are no longer read-only: an explicit write of the documented LED-mask
   function is admitted, the stage 02 ledger row records the protection effect as open, and every
   result states "protection effect unreviewed (UM21 p85)". Caller current limits and the
   `UNKNOWN` invalidation after an uncertain write are unchanged. Recorded in 08 item 4, the 08c
   row and 08 acceptance.

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this
directory. Nothing was compiled; no hardware or port was used.

## Round 3

Audit [`aud-20261119-191534-e56216`](audit.md#round-3) was checked finding by finding against this set's files
(byte-identical to the audited ones at `94138cd7`), prompts 57 and 77, the pack
[review](../../reference/moons_canopen/review.md), the page-marked UM21 extract, the rendered UM21 pages p16,
p21, p32, p34, p35, p37 and p56, the three EDS files and the owner header `adapters/owner/CanBusOwner.h`. The
cross-cutting part was applied once in `c4e5574f` and `6ff80ed1`
([shared round 4](../canopen_shared/audit_application.md#round-4-cross-cutting-findings-of-the-round-2-applications-and-round-3-audits-19-november-2026)):
canopen_shared 02 "State transitions" (halt-bit rule) answers F1's shared part, "Timed runs" (a stop that can
leave on an unresolved route) F2's, and prompt 57's bootstrap Enter Pre-operational most of F5. The family
stages now consume those rules by citation and add only MOONS' facts. Only this directory's files changed; no
pack, shared, main-sequence or source file was edited. MotorControl-RS was not consulted: no finding concerns a
common motion name.

Dispositions: 10 accepted, none accepted in part, rejected or deferred.

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 | Accepted | Old 04 items 7-9 (0x000B fixed, the halt-release question left open for S3 only), 05 item 10; shared 02 "State transitions" after `c4e5574f` (a stop word keeps a set halt bit unless the family's evidence shows quick-stop precedence; unknown halt keeps bit 8 set where the evidence admits that word, else the row refuses); rendered UM21 p16 (every state command coded in bits 7, 3, 2, 1, 0 only; transition 11 "Quick stop command"), p21 (released halt continues the set-point), p37 (bit 8 Halt in all five mode columns), p56 ("All implemented bits of the controlword are valid independent of the PDS FSA state"; bit 8 Halt beside the state bits), p32, p34, p35 (state commands sent with bit 8 set, 0x010F; Q halt 0x011F). No page states quick-stop precedence over the halt release; no UM21 example sends 0x000B or 0x010B | The evidence admits the word with bit 8 kept set, on the same unqualified basis as 0x000B (MO-D34). 04 item 8: "halt possibly set" (a candidate with bit 8 set, or the record unknown) and a bit 8 rule for every word that leaves Operation enabled: quick stop 0x010B, release 0x0106, disable voltage 0x0100 in that case, unqualified until 14a Q8. 04 items 7 and 9: the quick-stop row chooses 0x000B with halt known clear and 0x010B otherwise, under every identity outcome; the S3 reason now rests on MO-D34 alone (a shape would have to be 0x010B). 05 item 10 consumes stage 04's stop result (word, fresh state, mechanical effect not observed) instead of "ends disabled". Tests in 04 (halt clear, set and unknown predecessors under each outcome) and 05. 14a Q8 records the transitions of both words |
| F2 | Accepted | Shared 02 "Timed runs" after `c4e5574f` (admission needs the binding's S3 shape or an application-declared independent stop named in the result, else refusal before the first changing TX); shared 02 S10 (`NONE`: the route stays unresolved); `CanBusOwner.h` `admitUrgent` ("quarantined routes remain unavailable") and `admitBestEffortStop` (the only stop on an unresolved route, configured shape only); 02b items 5-6 (MOONS' `NONE`, no shape) | 04 item 10: after a timeout under `NONE` no stop frame leaves on the binding; the result says "no software stop on the unresolved route" and names the fallback. 06 item 4: a stop row is not a stop path; with no S3 shape the run is refused before its first changing TX (mode select and enable included) unless the application declares an independent stop, named in every result; the enable input counts only where wired and configured; at stop due after a lost start reply the run keeps `UNKNOWN` and `needsStop`. 06 acceptance: refusal without the declaration, and the owner disposition from start TX through lost reply, route timeout, stop due and the urgent attempt with zero stop frames |
| F3 | Accepted | Shared 02 "State transitions" (tables for enable, release, stop, fault clear and mode change, already-reached results); old 04 items 4, 5, 8; UM21 p16 (transitions 5, 6, 8, 9, 12, 14, 15 with their actions; transition 4 clears set-points), p21, p22, p35, p66 (0x2006 clears 0x603F and 0x200F, no state effect stated); AGENTS manual-arrow rule | 04 item 4: the release word is 0x0006 (p16 transitions 6 and 8: high-level power off "if possible"; 0x0007 keeps it, 0x0000 is the disable-voltage stop) with a source-state table (Operation enabled 0x0006 or 0x0106, Switched on 0x0006, already released nothing sent, Quick stop active, fault and Not ready to switch on refused) and settlement by a fresh 0x6041. 04 item 5: a fault-reset table for both methods (no TX without a fault or alarm, since 0x0000 outside Fault is a state command; 0x2006 refused while enabled or stopping because p66 states no state effect). 04 item 8: `prepareHaltRelease` with a table: mode 1 only, the recorded word with only bit 8 cleared, refused with an unknown or two-candidate record, in modes 3 and 4 (the run start belongs to stage 06), 6 and -1 (continuation not stated) and outside Operation enabled; a changing request without stop exemption. Deliver adds the operation and a family `halt-release` route; acceptance names every row. 14a Q4 names 0x0006 as the production word; Q8's resume uses the new operation |
| F4 | Accepted | Prompt 57 outcome definitions and admission by outcome; old 13 item 3 (a 0x2050 value UNCONFIRMED; typed 0x2100 only after "confirmed"); old contract identity bullet ("a confirmed layout"); 03 item 2 (references per row) | 13 item 3: the outcome is prompt 57's, never reclassified; a 0x2050 value matching a declared STF-C or SSDC-C reference with no TSM/TXM-C match is CONTRADICTED; another abort, a timeout or an unreferenced value is UNCONFIRMED without the declared match. Typed 0x2100 and every Part A read need an admitting outcome, not CONFIRMED alone; Part B's manual intake and model identification are named as separate gates. 13 acceptance: exact traffic for a matching UM21 candidate, an unreferenced string and a colliding declared match, with and without typed 0x2100. Execution contract: "an admitting outcome" replaces "a confirmed layout", and stage 13's further gates are named separately |
| F5 | Accepted | Prompt 57 after `c4e5574f` (targeted Enter Pre-operational to a node whose fresh heartbeat or guarding state reports Stopped is a bootstrap command under every outcome except CONTRADICTED; otherwise the node is stranded and recovery is the application's); UM21 p13 (heartbeat state 4 Stopped; commands 0x01, 0x02, 0x80-0x82), p14 (Stopped serves node and error control only; transition 7 Stopped to Pre-operational), p48 (0x1017 default 1000 ms, "0 if it not used"); 09 items 3 and 9 (no node guarding; node-0 Enter Pre-operational refused) | 09 item 1 consumes the bootstrap command with MOONS' facts: the heartbeat is the only state report, 0x1017 = 0 turns it off; a targeted Stop's result names its recovery (that command while 0x1017 was read nonzero in the epoch, otherwise "application only (power cycle)", since node-0 0x80 is refused on a MOONS' interface); the command revokes the node's S13 approval first; a node with no SDO answer and no fresh heartbeat is reported stranded, never Stopped by assumption. 09 acceptance: Stopped after rebinding and after a host restart (admitted on a fresh Stopped heartbeat with no current outcome, refused under CONTRADICTED, stranded without a heartbeat), the Stop result with 0x1017 nonzero, zero and unread, and a possibly sent Stop. No precondition was added before a Stop: the result names a power-cycle-only recovery instead of refusing (the owner's 2026-11-17 direction for user responsibility) |
| F6 | Accepted | Old 09 item 9 ("each declared MOONS' node"; 0x2060 watchdog); EDS-TSM `[2300]` UserWatchdog and no `[2060]`; AMP10 p48-p49 (SYNC Q in the 2010 layout); 13 items 1-2 (Part A read-only, no effect review); shared 02 S13 (approval covers the whole declared population; unknown effect refuses) | 09 item 9: the entry covers UM21 bindings only; a declared TSM/TXM-C node has unknown effects, so S13 refuses SYNC and node-0 NMT on its interface, also when it is only monitored or not the target, until stage 13 Part B supplies its entry; none of the entry's reads goes to its route. 09 acceptance: the mixed-layout case with zero SYNC and node-0 NMT TX. 13 item 3 says Part A supplies no S13 entry; item 8 lists the entry as Part B work |
| F7 | Accepted | Old 07 items 3, 4 and acceptance; prompt 77 item 2 (the guard on by default, settable per axis, an admitted result names a guard that was off); 14a Q10 (b) | 07 item 3: one rule in two parts. The direction check is the qualification alone, refused before prompt 77's guard lands and that guard afterwards (no second guard; guard-off results name it and the missing qualification); the method table, index declaration, input function and wiring declaration and the stop and travel plan stay required whatever the setting; Q10 never uses the guard-off path as first evidence. 07 item 4 now requires only the switch facts plus item 3's check. 07 acceptance: unknown and stale direction with the guard on (or not landed) and off, and the other prerequisites refusing with it off |
| F8 | Accepted | Old 07 items 6, 8, 9, 10; old 10 item 5 ("after every Q run"); UM21 p66 (0x2009 offset modes), p67 (0x200C zeroes position parameters); AGENTS invalidation rule | 07 item 12, coordinate retirement: before a moving-homing trigger, a 35 or 37 trigger or a 0x200C write is handed to the owner, the host origin and reference generations, prepared or queued position work built on them and bound host soft limits are retired; a lost reply, timeout, cancellation, halt or unobserved completion never restores them; a replacement only from item 8's qualified completion with a fresh 0x6064, or an acknowledged 35, 37 or 0x200C followed by fresh 0x6064, 0x607A and 0x607C reads. Items 9 and 10 cite it. 10 item 5 applies the same rule before the Q start word, with no replacement from a Q result. Tests in 07 and 10 |
| F9 | Accepted | 08 item 4 and block 08a; review section 11 ("changes current and motor model"); UM21 p84 (0x260D "used to set the motor", a model list only) and p86 (0x2612 named sub-indices without current semantics) | 08 item 4: a 0x260D or 0x2612 write carries the caller's current limits, leaves the binding's current `UNKNOWN`, and enable and motion refuse until fresh 0x2601 and 0x2603 reads lie within the limits; the readback is the drive's reported setting, not a measured current; not part of the quiescence guard and not settable. 08a row and acceptance (missing, above and within limits, uncertain acknowledgement) |
| F10 | Accepted | 14 items 3 and 4; 10 item 6 (no 0x6060 = -2 path); 14a Q11; 06 item 9, 11 item 6, 02 preflight, 13 Part B | 14 item 3: each operation is qualified only through its owning stage's production route or an approved 14a evidence path, otherwise NOT RUN with the gate named (torque, watchdog arming, withheld store or restore, Part B); qualification opens no gate. 14 item 4: mode -1 programs owner-supplied; mode -2 unsupported and NOT RUN |

### Unresolved questions

1. **Answered by the code and the shared contract.** Under `SdoReuse` `NONE` a timeout leaves the route
   unresolved; `admitUrgent` keeps it unavailable ("quarantined routes remain unavailable",
   `adapters/owner/CanBusOwner.h`), and the only frame that may leave on an unresolved route is
   `admitBestEffortStop`'s configured shape, which a MOONS' binding does not have (S3 default deny). So no
   ordinary urgent halt reaches the drive after a lost start reply. Shared round 4 makes the run require an
   application-declared independent stop for that case; stages 04 item 10 and 06 item 4 now say so and test it.
2. **Answered by the lead decision of round 2, which stands.** The explicit write of the documented LED-mask
   function of 0x2610 and 0x221A is admitted with "protection effect unreviewed (UM21 p85)"; the execution
   contract's reviewed-effects rule now states that one exception in one place and that no other object is
   written on that basis. Stage 08 item 4 keeps the details.

The coverage gaps are the executing stages' intake work, as before. For the device claims used here the rendered
UM21 pages named above were reopened.

### Decisions for the lead or owner

1. The release word is now fixed as 0x0006 from UM21 p16's transition actions (F3); 0x0007 and the disable-voltage
   stop stay separate. Confirm, or name another word for stage 04's review.
2. 0x2006 (`DSP_CLEAR_ALARM`) is refused in Operation enabled and Quick stop active because UM21 p66 states no
   state effect (F3). It could become a settable guard like stage 08's quiescence guard if the owner prefers.
3. The bit 8 rule also covers release (0x0106) and disable voltage (0x0100), not only the quick stop (F1); it
   follows the shared rule's reasoning for every word that leaves Operation enabled.

### Round 3 lead decisions, 19 November 2026

The three questions above were decided by the lead and applied:

1. **Confirmed.** The release word is 0x0006 (UM21 p16 Shutdown, bits 7, 3, 2, 1, 0 = 0 x 1 1 0, transitions 2,
   6 and 8), sent as 0x0106 under the bit 8 rule. Recorded in 04 item 4.
2. **Changed.** 0x2006 while enabled (the Operation enabled and Quick stop active row of 04 item 5) is an
   alarm-clear guard, on by default and settable per binding, following prompt 77's guard pattern (an admitted
   result names a guard that was off) and the owner's 2026-11-17 direction. With the guard off, 0x2006 = 1 is
   sent and the result states that a pending target may resume. Recorded in 04 item 5 and 04 acceptance;
   this supersedes the refusal described in the F3 row above.
3. **Confirmed.** The bit 8 rule covers release and disable voltage as well as the quick stop: keeping halt
   set cannot start motion.

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this directory.
Nothing was compiled; no hardware or port was used.

## Round 4

Audit [`aud-20261119-193918-83d072`](audit.md#round-4) was checked finding by finding against this set's files
(byte-identical to the audited ones at `a6b58cf1`), prompt 57 and canopen_shared 02 "Operation admission" as
revised in `328651b6`, the shared [round 4 themes 6-8](../canopen_shared/audit_application.md#round-4-cross-cutting-findings-of-the-round-2-applications-and-round-3-audits-19-november-2026),
the pack [review](../../reference/moons_canopen/review.md) (halt, interlock and enable-input entries), the
page-marked UM21 extract and the rendered UM21 pages p17, p18 and p22 (this round) and p16, p21, p32, p34, p35,
p37 and p56 (round 3). Extract pages used besides: p13, p19, p23, p31, p33, p63, p67, p84 and p86. Only this
directory's files changed; no pack, shared, main-sequence or source file was edited. MotorControl-RS was not
consulted: no finding concerns a common motion name.

Dispositions: 7 accepted, 1 accepted in part (F3), none rejected or deferred.

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 | Accepted | Old 04 item 1 (Operation enabled row: nothing sent, the record carried), item 3 (mode change "when the record shows halt set", undefined for two candidates); 06 items 2 and 9 and 10 item 3 (enable with halt set, then setup writes); UM21 p32, p34, p35 (enable with 0x010F, start by clearing halt) | 04 item 8 defines the **known-halted** predecessor: a fresh Operation enabled read and a known record with bit 8 set in every candidate; an enable result, a mode change or a stop result is not that evidence. 04 item 1's already-enabled row says so; item 3 admits a change into 3, 4 or -1 only from it. 06 item 2 (and item 9 for mode 4): halt admission from fresh 0x6041 and 0x6061 before the first changing TX; a drive not enabled is enabled with 0x010F, an already enabled one must already be known halted; otherwise the run is refused before TX, naming stage 04's halt, and no setup write leaves until the record is known halted. 10 item 3 applies the same rule before the mode -1 write, the 0x2007 write and the start, also with the drive already in mode -1. Tests in 04, 06 (same-mode request with the three records) and 10 (mode -1 select and already in -1) |
| F2 | Accepted | Old 04 item 9 halt row (admitted on a known record; 0x011F after any position, homing or Q trigger, also after a lost reply); rendered UM21 p17 (bit 4 "Toggle this bit from 0->1 to clock in a new set point"), p22 (bit 4 "Start or continue homing procedure", bit 8 = 1 "Stop axis"), p16 (Enable operation 0 1 1 1 1 makes transitions 4 and 3+4; transitions 5 and 8 also by "local signal"; Quick stop 0 x x 0 x has no enabling transition); p19 (0x001F then 0x000F clears a set-point), p23 and p35 (0x011F halts after 0x001F), p32 and p34 (0x010F stop), p31 and p33 (no bit 4 function in modes 3 and 4); review (interlock to Switch on disabled, UM21 p63) | 04 item 8, "Halt word": bits 0-3 and 8 set; bit 4 never raised. The recorded word with bit 8 set only when the record is known, every candidate has bit 4 set and the candidates agree in bits 4-6 and 9 (0x011F after an acknowledged 0x001F); otherwise 0x010F, also with the record unknown. A falling bit 4 makes no edge in modes 1, 3, 4 and 6 by the pages above; in mode -1 its effect is not stated, so a Q halt sent as 0x010F records it for 14a Q11. 04 item 9, "Halt source state": the halt first reads 0x6041 and 0x6061 through the urgent path and is sent only from Operation enabled in mode 1, 3, 4, 6 or -1; any other state or mode, or no reply, refuses before TX, because the halt word is the Enable operation command from Ready to switch on and Switched on, which local signals can reach. The refusal names the quick stop and the application-owned fallback; the remaining window between read and TX is recorded in the result. The identity exemption is unchanged. 06 item 4, 07 item 6 and 14a Q10 cite the word. 04 acceptance: acknowledged and lost trigger replies (both outcomes, 0x010F), an unknown record, and no TX after a state change without a host word or without a read reply |
| F3 | Accepted in part | Old 08 item 4 (quiescence guard settable; the 0x260D and 0x2612 current rule has no off setting but acts only on the next enable and motion); UM21 p84 (0x260D "used to set the motor") and p86 (0x2612) state no point at which the drive applies the change; p67 (0x200B bits 0 and 14); the owner's 2026-11-17 direction for optional, settable guards (round 2 decision 4, round 3 decision 2) | 08 item 4 adds the **motor-data de-energized guard** for 0x260D and 0x2612 writes only: a fresh 0x6041 in Switch on disabled or Ready to switch on and 0x200B bits 0 and 14 clear, read within the operation. It is separate from the quiescence guard (switching that one off leaves this one on), on by default and settable per binding; with it off the write is sent and the result names the guard and states that the current may have changed on an energized drive. The caller limits, the `UNKNOWN` current and the enable and motion refusal stay. Not made non-bypassable, as the reviewer recommended, per the owner's direction and the lead's instruction for this round. 08a row and acceptance (enabled idle and running drives with the general guard off, both guards off, partial and uncertain writes) |
| F4 | Accepted | Old 09 item 9 revocation bullet ("only fresh reads, with the NMT state the node then reports, requalify it"); shared 02 S13 (refuses unknown or stale effects); UM21 p13 (heartbeat states 0, 4, 5, 0x7F; NMT commands have no reply) | 09 item 9, new "Settling" bullet: requalification waits until every NMT command this owner admitted to the node or to node 0 is settled; while one is queued, in flight or sent without a later state report the approval stays revoked and reads taken meanwhile do not count. Settled by withdrawal before TX, a heartbeat after TX reporting the commanded state, boot-up after a reset, or a later admitted command settling; a heartbeat with the old state leaves it unsettled; with 0x1017 = 0 the node stays revoked until boot-up. 09 acceptance: reapproval with fresh Pre-operational reads refused while a targeted Start is queued and after its TX before an Operational heartbeat; the broadcast Start now asserts zero NMT TX (the old text grouped it with the targeted case) |
| F5 | Accepted | Old 04 item 8 halt-release row ("completion follows stage 05 item 8 against the retained target"); 05 item 8 (completion anchored at a trigger); rendered UM21 p18 (bit 10 with halt = 1 means "Velocity of axis is 0"); p21 (the set-point continues after release); p16 transition 4 | 04 item 8: the halt release reports its acknowledgement and any continuation a later read shows, and claims no move completion. 05 item 10 owns resumed-target completion: the move retains its target and record from the trigger until observed completion, a new move, an unknown record, leaving Operation enabled or stage 07 item 12's retirement; after an acknowledged release, completion needs bit 10 low after the release, then high with halt clear and bit 12 low; a high bit 10 without that fall gives `NOT_OBSERVED`. Tests in 04 and 05 |
| F6 | Accepted | Old 09 item 1 (the `nmt` route and the bootstrap Enter Pre-operational for MOONS' bindings generally); 03 item 2 (an AMP10 declaration refused for everything but the probe until stage 13); 13 items 1, 3 and 8 (Part A read-only); prompt 57 ("a family may refuse more with recorded evidence") | 09 item 1: the `nmt` route is for UM21 bindings only; every targeted NMT command to a TSM/TXM-C binding, the bootstrap Enter Pre-operational included, is refused before TX until stage 13 Part B admits it. 13 item 8 lists that route as Part B work. 09 acceptance: the bootstrap 0x80 to a Stopped TSM/TXM-C declaration with no current outcome gives zero TX |
| F7 | Accepted | Old 07 items 9, 10 and 12 (stationary admission named but undefined; 0x200C with retirement only); UM21 p67 (0x200B bits 4 Moving, 6 Stopping, 10 Homing, 14 Q running; 0x200C zeroes position parameters), p16 (transition 4 clears set-points; transitions 11, 13, 14), p21 (a released halt resumes the set-point) | 07 item 10: a stationary predecessor table from fresh 0x6041, 0x6061 and 0x200B reads: admitted from Switch on disabled, Ready to switch on or Switched on with the four 0x200B bits clear, or from Operation enabled with them clear and no set-point possibly pending; refused with a possibly pending set-point or an unknown record, with any of the bits set or an operation of the binding in flight or `UNKNOWN`, and in Quick stop active, fault states or without a reply. Retirement still precedes the TX; a replacement reference also needs a later 0x200B read with the bits clear (item 12). Item 9 uses the same table for 35 and 37. 07 acceptance |
| F8 | Accepted | Old execution contract "Hardware" (firmware strings and 0x2020 before any later bench step); old 14a entry ("its recorded identity"), "Before first power" list and Q1-Q3; 03 items 1, 2, 5 and 8 (0x2020 is a manufacturer-area, layout-typed read) | Execution contract "Hardware": the pre-power facts (board, transceiver, label and part number, motor, wiring, switch positions, RS-232 tool, authorization) are separate from the firmware strings and the 0x2020 part, which 14a Q1-Q3's read-only sequence establishes; those steps alone precede them. 14a entry: run with the recorded pre-power identity; Q1's passive observation and stage 03's probe, Q2's bootstrap identity reads, then Q3's reads under the layout admission are the only steps before the CAN identity is recorded. Q3 now reads 0x2020 against the switch and the active node ID |

### Shared change after the audit (`328651b6`)

- **Theme 6, unknown halt state.** Applied to stage 04: with the record unknown a stop word sets bit 8 and the
  result records the unknown halt state; no stop row refuses for that reason alone, since no UM21 page gives bit 8
  another meaning (p37, p56). The halt row is no longer refused for an unknown record (it sends 0x010F after its
  fresh source-state read, F2). The quick stop (0x010B), release (0x0106) and disable voltage (0x0100) already set
  bit 8 with halt possibly set; the disable voltage stays refused under a refusing outcome only because it is not
  exempt from identity, which the theme does not change.
- **Theme 8, undecided reference.** Stage 03 item 2 called the TSM/TXM-C abort for a UM21 declaration
  CONTRADICTED. With the declared row's 0x2050 field aborted that reference is undecided, so the outcome is
  UNCONFIRMED without the declared match (it refuses the same changing work; the bootstrap Enter Pre-operational
  differs). The example was corrected; stage 13's CONTRADICTED case (a complete 0x2050 value for a TSM/TXM-C
  declaration) is a decided non-match and stands.
- **Theme 7, homing as a host-ended run.** Not applied: stage 07 item 6 still states its own caller stop and travel
  plan rather than citing the shared latest interruption time, stop budget, transfer cap and stop path. It is
  outside this round's findings and the stage 04 alignment the lead asked for (decision 4 below).

### Unresolved question

1. **Answered by the files.** Stage 06's torque admission means an admitting outcome under prompt 57 (stage 03
   item 2) with a current match of the declared SSDC-C row's reference: CONFIRMED, or UNCONFIRMED with that match
   (a collision or a candidate without references), whose result names every matching row. Nothing in the set
   justifies CONFIRMED only: the execution contract's identity bullet requires "an admitting outcome" since round 3
   F4, and prompt 57 lets a family refuse more only with recorded evidence, of which there is none for torque.
   06 item 7 now says so; 06 acceptance tests a colliding declared match, still refused by the 0x6087 preflight.

The coverage gaps are the executing stages' intake work, as before. For the device claims used here the rendered
UM21 pages p17, p18 and p22 were opened this round.

### Decisions for the lead or owner

1. F3: the de-energized guard for 0x260D and 0x2612 is settable (on by default), not non-bypassable as the reviewer
   recommended, following the owner's 2026-11-17 direction and this round's instruction. Confirm.
2. F2: a halt now costs its own fresh 0x6041 and 0x6061 reads before TX, which the caller's stop budget must cover
   (stage 06 item 4), and a local-signal transition between that read and the TX is recorded, not excluded.
   The alternative is to offer only the quick stop as the urgent stop and keep halt as a non-urgent pause.
3. F2: after a lost Q start reply the halt sends 0x010F, lowering bit 4 with an undocumented Q effect (UM21 p35
   shows only 0x011F), instead of refusing with the quick stop named. Confirm, or prefer the refusal.
4. Theme 7: stage 07 item 6 should consume canopen_shared 02 "Timed runs" for homing (a caller-supplied latest
   interruption time, the stop budget, the transfer cap and a stop that can still leave after a lost reply,
   which for MOONS' without an S3 shape means a declared independent stop). Authorize that edit or assign it.
5. Theme 8: the stage 03 example was corrected beyond the stage 04 scope because it contradicted prompt 57 as
   revised. Confirm or revert.

Checks: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean for this directory.
Nothing was compiled; no hardware or port was used.

### Round 4 lead decisions, 19 November 2026

The five questions above were decided by the lead and applied:

1. **Confirmed.** The motor-data de-energized guard for 0x260D and 0x2612 is settable, on by default, and an
   admitted result names it when off (08 item 4, unchanged from the F3 row).
2. **Confirmed.** The halt reads 0x6041 and 0x6061 before its TX, and a state change between that read and the
   TX is recorded in the result, not excluded (04 item 9, "Halt source state").
3. **Changed.** In mode -1 the halt is sent only when it lowers no bit 4: 0x011F after an acknowledged 0x001F,
   or 0x010F before any start. After a lost Q start reply the halt row refuses before TX, because a falling
   bit 4 has no stated Q effect (UM21 p35), and the result names the quick stop 0x010B as the available stop,
   which stays admitted. No 0x010F is sent in that case. The same refusal covers an unknown record in mode -1,
   which has the same undocumented bit 4 effect. Recorded in 04 items 8 and 9 and 04 acceptance, and 10 item 3
   and 10 acceptance. This supersedes the 0x010F Q halt in the F2 row above.
4. **Authorized and applied.** 07 item 6 consumes canopen_shared 02 "Timed runs" for homing by citation: the
   stop and travel plan is that bullet's caller-supplied latest interruption time, an optional travel bound from
   0x6064 observations of bounded age, the stop budget, the transfer cap at the interruption time and the stop
   path. Without an S3 shape a run needs a declared independent stop, or it is refused before its first changing
   TX. Tests in 07 acceptance.
5. **Confirmed.** Stage 03's identity example stays aligned with prompt 57's undecided-reference rule.

Checks after these changes: `python scripts/check_promptset.py` reports no errors; `git diff --check` is clean
for this directory.
