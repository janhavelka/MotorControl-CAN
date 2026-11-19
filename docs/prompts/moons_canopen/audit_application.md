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
