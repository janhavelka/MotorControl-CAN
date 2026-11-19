# Application of the Lichuan DS2-C promptset audit, 19 November 2026

Each finding of [audit `aud-20261119-180844-aac472`](audit.md) was checked against this directory at
`0ab4e181` (`main`), the [reference review](../../reference/lichuan_ds2_canopen/review.md), the original
EDS files and, where a device claim decided the change, rendered PDF pages. The shared contract revised in
`1043608b` ([canopen_shared 02](../canopen_shared/02_profile_integration_contract.md), "Operation admission",
"First-contact questions", the S3 and S13 bullets;
[round 2](../canopen_shared/audit_application.md#round-2-family-audits-19-november-2026)) and prompt 57's
Identity policy bullet as tightened in `0ab4e181` were consumed by citation, not restated.
Following the user's direction of 2026-11-19, fixes stay with the findings. Only files of this directory
changed; nothing was compiled and no hardware or port was used.

Dispositions: 16 accepted (F1, F2 critical; F3-F11, F13-F16 high; F12 medium), none accepted in part,
rejected or deferred.

## Findings

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 | Accepted | canopen_shared 02 S3 bullet ("default deny lands in the S2 commit, before any further family's first binding can be admitted"); README S3 row; canopen_shared 01 service-engine row (`validBestEffortStop` accepts `6040:00=000B` on every channel); old 02b header ("seam S3 stays with 04") | 02b header, item 3 (deny in the S2 commit or DS2 cases added to a landed one; direct `admitBestEffortStop` tests on an unconfigured DS2 binding, an unresolved route after a timeout and a rebound channel; Lichuan unchanged) and acceptance; 04 item 7 now only the opt-in shape under `DS-DEC-3`; README stage and seam tables |
| F2 | Accepted | Prompt 57 Identity policy bullet at `0ab4e181` (outcome definitions, vendor-object rule, new motion rule: CONTRADICTED refuses every changing request, no override); `lichuan_c_extensions/execution_contract.md` "Identity outcomes" (now a pointer to prompt 57); old DS2 table row allowing standard-object operations on CONTRADICTED after acceptance | Execution contract identity section: cites prompt 57 as the definition and removes the DS2 restatement of CONTRADICTED; references stated as (`1000`, `1018:01`, `1018:02`) tuples; one operation-by-outcome table (bootstrap reads, configuration reads, changing requests, `2406h`) with rows for no current outcome, a declared-model match, no match (differing `1000` included), an unread product field and CONTRADICTED (prompt 57's rules, no acceptance); the DS2 caller acceptance was then removed and stops exempted (lead decisions 1 and 2); foreign-fake-server test with acceptance set for every changing stage. Boundary-table row and README relationship text point to prompt 57; 03 item 2 consumes it |
| F3 | Accepted | Rendered DS2C p12 (`607Fh` UNSIGNED16 -65535..65535) and DS2C p28 object table (U32 0..2147483648); MINI mode and object tables in the extract (same pair); rendered ISL60 p16 and ISL80 p15 (`607Fh` UNSIGNED32 0..2147483648), ISL object tables U32; `[607F]` `DataType=0x0007` in DS2EDS, IBLEDS and ISLEDS; contract 02 write preflight | 02 item 4: `607F` width withheld in DS2 and MINI, four bytes in ISL once the ledger confirms the agreement; 06b items 2 and 4 (preflight covers mode select; DS2/MINI torque withheld until model-scoped evidence; ISL four-byte download), acceptance and deliverables. Refinement: the ISL scopes have no width conflict, so torque stays implementable there |
| F4 | Accepted | Review "What a profile can implement" (no vendor-object write before live reads, firmware identity and a qualified fixture); IBL60 p13 extract (`2406h` UNSIGNED16, absent from IBLEDS); execution contract boundary row listing `2400`-`2407` as "must not exist" | Boundary row now forbids Lichuan C's definitions, not the index, and names `2406h` as DS2's own manual-only row; 02 item 4 and 06b item 4: the write is withheld in the ledger until model- and firmware-scoped evidence of meaning and effect; identity and a fresh read stay necessary checks; acceptance tests a readable `2406h` with an admissible outcome giving zero changing TX; 10 acceptance |
| F5 | Accepted | Rendered DS2C p12 (bits 0..2 projections 000/001/011/111 for words 0/6/7/15; bit 7 alarm); contract 02 state-transitions bullet; old 04 item 2, 06 item 1, 08 item 1 versus 08 acceptance | 04 item 1: source-state table first (dialect, low bits, alarm, mode, freshness, intent; first word, already satisfied or refusal; undocumented patterns, alarm, stale refuse) and the `released`/`enabled` predicates; enable from a fresh 111 returns already enabled with no TX; first-frame tests per row. 05, 06, 06b and 08 consume the predicates. 08 policy chosen: released only, an enabled stationary drive refused (activation timing of 6000-group objects unknown) |
| F6 | Accepted | Old 05 item 6 ("after acknowledgement or a terminal"); principles pack CiA 402 transition 16 (Quick stop active to Operation enabled on enable operation, with `605A` 5-8); DS2 has no `605A`, so its post-stop reaction to word 15 is unknown | 05 item 6: `DS-DEC-5` enumerates eligible evidence and generations; stop admission, cancel, expiry, timeout or lost reply, boot-up, reset, rebind and invalidation retire a pending rearm; never cleanup; a later move writes 15 after its own admission; acceptance asserts traffic for a stop racing a rearm, an unknown trigger, cancel and expiry |
| F7 | Accepted | Review DS-D09 and DS-D10; rendered DS2C p12 (`60FFh` UNSIGNED32 0..2147483648) | 02 item 4, 05 item 4, 06 item 2: unqualified range 0..2147483647 on the exact quantized value before narrowing; 2147483648 rejects; a signed declaration names its range; acceptance tests both edges, rounding across them on native and common paths and zero changing TX |
| F8 | Accepted | 02 item 6, old 03 item 8, execution contract boundary row, 05 item 7; `SdoTransfer.h` (`repeatableReadEffect` needs a reviewed or accepted effect); Lichuan `Position.h` observation cycles resting on DEC-1 | 03 item 8: fixed named reads versus polling; 03 prepares the `DS-DEC-1` record (objects, scope, repeat policy, budget, `selected_by`); markers are a separate decision (02b item 5, README S10 row, boundary row). Without it: 05 ends acknowledged with `NOT_OBSERVED`, 07 refuses homing (its search has no host-visible bound), 04 to 06b use only named single reads. Tests for both cases in 03, 05 and 07; 12a Q4 path |
| F9 | Accepted | Contract 02 one-deadline bullet (no changing TX after `now >= deadlineUs`); old 06 item 5 and 06b item 6 | 06 item 5: the run counts from the start word's admission; at preparation and before the start word, overflow-checked `now + durationUs` plus the stop dispatch budget must fit the deadline, otherwise no start word; after the deadline nothing is sent, the stop obligation stays with never-sent versus possibly-sent; 06b item 6 consumes it; acceptance tests cumulative delay, equality and resumed service |
| F10 | Accepted | canopen_shared 01 S7 bullet (the first header's commit carries its install rule, package-test entries and per-header compile); old 02 items 7-8 and 02b item 8 | 02 item 7 adds the install rule, `test/package_test.py` entries and per-header C++11/C++17 compiles in the generated headers' commit; item 8 scopes both operation checkers with disjoint-and-complete regressions; `check_lichuan_ds2_operations_test.py` registered; 02b item 8 extends lists only for its files; README rows |
| F11 | Accepted | Prompt 57 CONFIRMED definition (unique among supplied references); `[1018sub*]` sections: DS2EDS and DS-C EDS end at sub3, IBLEDS and ISLEDS have sub4; the pairs' model counts (manual nine, DS2EDS/ISLEDS five, IBLEDS two) | 03 item 1: `1018:01`..`04` read only when announced and sourced in the scope; short, oversized and contradictory counts. 03 acceptance: synthetic truth-table tests separate from production-claim tests asserting the actual UNCONFIRMED and CONTRADICTED outcomes and matching lists; MINI manual-only. Execution contract states the consequences (no documented pair gives CONFIRMED; another model's pair is CONTRADICTED until a MEASURED reference exists) |
| F12 | Accepted | 09 item 2 (PDO read plan), 09 item 6 (`1017`), execution contract console paragraph (routes wait for S4) | 12a header: evidence path per question under contract 02 "First-contact questions"; production route is the `ds2` console (S4) or an installed example calling the public API, else NOT RUN, no raw path. Q2 split into Q2a (03 snapshot, identity-admitted) and Q2b (09 network snapshot, only Q11 needs it); order rule updated |
| F13 | Accepted | 05 item 3 (`WRITE_NATIVE` needs a declared `RampKind`); contract 02 "First-contact questions" | 12a Q6: manufacturer clarification, applicable evidence or a separately assigned reviewed experiment with permitted values, snapshot, observation and restoration boundary; otherwise NOT RUN; no provisional declaration as the result |
| F14 | Accepted | Review "Host loss produces no documented drive action"; DS-D09, DS-D10, DS-D11 | 12a Q8 split into Q8a (position, a bound effective in mode 1) and Q8b (velocity; `607D` bounds no speed in mode 3); bounds must hold without host service; the clamp or stop under investigation never counts. Q10 states the same and admits torque only for models 06b admits. 12 item 3 cites Q8a/Q8b |
| F15 | Accepted | Review DS-D25 (PG-IBL60 DC 24..50 V versus IBL60 p5, p7 DC 24..70 V) and DS-D26 (BRK+/BRK- versus BK-/BK+ braking-resistor terminals, FunOUT.9); DS2C p5 and MINI p7 extracts (24..70 V) | 12a item 1: per-model supply from the model's own manual and page with the motor voltage code; a conflict needs a documented safe choice or manufacturer clarification before power, never the higher limit; input interface, DO loads and brake versus braking-resistor terminals in the same sign-off |
| F16 | Accepted | `CanBusOwner.h` `admitNmt` ("command, physical bus/address or explicit broadcast"); canopen_shared README S13 row (prompt 51 refuses only on a foreign producer); contract 02 S13 bullet | 09 item 7: refusing a DS2-targeted SYNC does not stop another family's or an armed schedule; DS2 shares no interface with another family until S13 lands, then adds DS2 cases to S13's producer tests. 11 item 2 and acceptance: positive shared-interface tests BLOCKED until S13 lands, which now covers SYNC and node-0 NMT (lead decision 3). Execution contract owner bullet and README tables |

## Reviewer's unresolved questions

1. **Answered by the files at `0ab4e181`.** The canopen_shared README table records no seam claimed or
   landed (S1-S3, S6-S8, S10-S14 not landed; S4 and S5 pending on prompts 64 and 65). `PROGRESS.md` lists
   prompts 57, 63, 64, 65 and 77 as Pending and 51 as Partial. So every `ds2` console item stays BLOCKED on
   S4, package components wait for S5, and the shared-interface positive tests wait for S13 (F16). Stage 01
   re-records this at the execution HEAD.
2. **Answered: none.** MINI has no EDS and its manual prints both `607D` rows as `:01`, like every family
   manual; only DS2EDS, IBLEDS and ISLEDS have `[607Dsub2]`. 03 item 5, 07 item 7 and 12a Q2a now read `:02`
   only where the scope's EDS sources it; MINI reads `:01`.
3. **Fixed.** 02 item 3 now records both parts of DS-D30: the `60FD` default and the `P02-34` (DS2C p22) versus
   `P0B-34` (MINI p24) monitor designation as a `manual_only` claim.
4. **Answered by the communication practice.** C11 bounds every queue: the event queue may lose entries, each
   counted, and never loses a terminal; C14 reports overflow as an event. 09 item 8 now delivers each EMCY into
   the application's bounded event queue with overflow counted and reported, and its acceptance tests it.

## Lead decisions, 19 November 2026

The lead answered the open questions of this application the same day; prompt 57 and the canopen_shared README
were revised to match. Applied in this directory only:

1. **Stops on CONTRADICTED.** Prompt 57's Identity policy bullet now exempts explicitly requested stops (the
   profile's reviewed quick stop or halt); the result carries the contradicted identity and claims no standstill.
   The execution contract's admission table gains a stop column: 04's `QUICK_STOP` is admitted under every outcome,
   still through 04's source-state table; the foreign-server tests expect it admitted with the identity attached.
2. **No caller acceptance.** Prompt 57 now says no caller acceptance replaces a reference; the application supplies
   a MEASURED reference for its specimen when the documented values conflict. The DS2 acceptance is removed from the
   execution contract (table rows and paragraph), 03 item 2 (`ds2 identity accept|clear`), 06b acceptance and 12
   item 6; UNCONFIRMED without a declared-model match, a differing `1000` included, refuses changing requests and
   `2406h` until such a reference or a corrected declaration exists.
3. **NMT broadcasts on a mixed interface.** Seam S13 now covers SYNC and every NMT command to node 0 on a mixed
   interface. The execution contract owner bullet, 09 item 7, 11 item 2 and its acceptance, and the README stage and
   seam rows consume S13 for both; no DS2-local guard. Until S13 lands DS2 runs on its own interface.
4. **`DS-DEC-1`.** It stays an owner or lead decision at implementation time, marked for the owner's review
   (03 item 8). Until it is selected, position completion is `NOT_OBSERVED` and homing is refused.

Still open: torque is admissible only for ISL60 and ISL80 (after stage 02 records the width agreement); DS2,
DS2-Mini and IBL torque need vendor clarification or qualified fixture evidence for the `607F` width or the
`2406h` write.

Checks: `python scripts/check_promptset.py` reports no error in this directory; `git diff --check` is clean for
it. Nothing was compiled; no hardware or port was used.

## Round 2, 19 November 2026

Each finding of [audit `aud-20261119-184724-17daf9`](audit.md#round-2) was checked against this directory at
`a7656efb` (`main`), the [reference review](../../reference/lichuan_ds2_canopen/review.md), the pack's extracts and
EDS files, and the Lichuan C headers where reuse decided the change. The lead's cross-cutting changes in `a7656efb`
were consumed by citation, not restated: prompt 57 "Admission by outcome" and the stop exemption's scope,
canopen_shared 02 "Identity and device evidence", the "Operation admission" timed-runs bullet, the S3 and S13
bullets and "Registration acceptance"
([round 3](../canopen_shared/audit_application.md#round-3-cross-cutting-findings-of-the-family-round-2-audits-19-november-2026)).
Only files of this directory changed; nothing was compiled and no hardware or port was used.

Dispositions: 7 accepted (F2-F6, F8 high; F7 medium; F3 to F7 largely by the shared changes), 1 rejected (F1, lead
decision), none accepted in part or deferred.

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 | Rejected as policy (lead decision below); its test recommendation applied | Prompt 57 "Admission by outcome" at `a7656efb` (a declared-model match admits because the declaration selects; the result names every matching model); canopen_shared 02 test bullet (an ambiguous match that includes the declared model admits); shared round 3 theme 2; engineering direction of 2026-11-17 (no lock-out, the user owns the declaration); review "Status-word dialects" (IBL60 table versus DS2C table for IBL80 under one IBLEDS pair) | Admission kept. Execution contract: the admitting row says every interpretation uses the declared model's tables, and a paragraph records the decision and the sibling differences. 03 item 4: the dialect comes from the declared model only, also when siblings match. 03 acceptance: cross-sibling tests through the real owner (IBL60 on an IBL80-behaving server and the reverse, DS2 on an ISL-dialect server): UNCONFIRMED naming every matching model, admitted, decoded with the declared dialect only; a sibling-only alarm bit is shown with the declared reading, the raw word and both models |
| F2 | Accepted | DS2C p11 (bit 4 "start new target position, rising edge"; examples 15 to 31/95/63/127 and 15 to 11; bit 5 0 finishes the current profile before updating) and DS2C p13 (bit 4 0 to 1 starts homing, 1 to 0 interrupts), the same text in IBL60 p12/p14, MINI p13/p15 and ISL60 p15/p16 (extracts); `[6040]` `AccessType` WO in DS2EDS l.1199-1202, IBLEDS l.1073-1077 and ISLEDS l.1073-1077 versus RW in DS2C p27 (DS-D08), so the word cannot be read back; Lichuan C `Position.h` (`control`: the latest acknowledged `6040` exactly 000F; "A move never writes 000F to make one. After a trigger, a stop or an uncertain word none exists") and the `Actions.h` file comment (the application's session supplies the history) | 04 item 10 (new): controlword history and trigger predecessor, reusing the Lichuan C design: the caller's latest acknowledged `6040:00` under one continuous reservation and current generations, exactly 0x000F, then a fresh `enabled` report; three establishing transitions (04's enable ending in an acknowledged 0x000F, 05's `DS-DEC-5` rearm, 07's explicit interrupt); its invalidators; "already enabled" creates none; no repair word. 04 item 3, Deliver and Acceptance. 05 item 5 consumes it; 05 item 6's round-1 sentence "a later move that finds bit 4 high writes 15 as its own first step" is replaced by "a move never writes 15 to make one", because no read can find bit 4 and, after a cancel or expiry, that write followed by a trigger with bit 5 0 could queue a new target behind a running profile; acceptance cases. 07 item 3 consumes it; 07 item 10 admits the interrupt only after this binding's acknowledged 0x001F and makes its acknowledged 0x000F the next predecessor (consecutive homing); acceptance cases. README 04 row |
| F3 | Accepted | DS2C p11 and the MINI p13, IBL60 p12 and ISL60 p15 tables: 15 to 11 appears only in the position-mode table; review "Other controlword use" (no `605A`, reaction undocumented); canopen_shared 02 timed-runs bullet ("a run whose mode and reachable states have no reviewed stop row is refused before its first changing TX") and S3 bullet (opt in only with a review of the word's reaction from every reachable state; CP-D10) | 04 item 6: `DS-DEC-2` rows per mode and predecessor keyed by item 10's history, with basis and the reaction as undocumented. 04 item 7: the S3 opt-in needs `DS-DEC-3`'s every-state review; no DS2 source documents those reactions, so it rests on CP-D10's command reading and says so, else the default deny stays; eligibility and S3's unchanged reporting. 06 item 5 and 06b item 6: a run without a mode-3 or mode-4 row is refused before its first changing TX, mode select included; neither a valid encoding nor a fake server accepting 0x000B clears it; without the opt-in an unresolved route gets no best-effort TX. Acceptance in 04, 06 and 06b; README rows |
| F4 | Accepted, by the shared change | canopen_shared 02 timed-runs bullet at `a7656efb`; `include/MotorControlCAN/VelocityOperation.h` (`durationUs` from successful preparation); `profiles/lichuan_c/Velocity.h` (`requiredStopBudgetUs`, `stopDueUs` as the run's service cutoff, `needsStop`, phase `STOPPING`) | 06 item 5 rewritten to consume the bullet: preparation-anchored duration (replacing the start-word anchor of round-1 F9), the budget checked at preparation, every run transfer's deadline at most stop due and no enable word at or after it, so 04's "in-flight request settles first" waits for nothing of the run; the stop is the application's separate urgent stop with its own deadline. 06 item 1 and 06b items 2 and 6 aligned. 06 acceptance lists the shared timed-run cases (an observation in flight near stop due, `now` equal to stop due, a lost start reply, service after stop due, a queue-pressured stop); 06b inherits them |
| F5 | Accepted | Prompt 57 at `a7656efb` (an NMT command to the binding is a changing request, never the stop exemption); canopen_shared 02 S13 bullet (node-0 NMT at the owner's NMT admission, default deny); principles pack section 3 (Pre-operational: SDO but no PDO; Stopped: only NMT and error control; command specifiers); DS-D32 (no NMT effect documented); old 09 item 5 and the foreign-server test list (04 to 08, 10) | Execution contract: the changing-request column names targeted NMT; a command-by-command NMT table (Start, Stop, Enter Pre-operational and both resets: changing requests plus a fresh `released` report and no pending operation on the binding, with the effect to record; node 0 refused by S13's default deny); the stop paragraph says an NMT command is never the stop; the foreign-server tests cover 04 to 10. 09 item 5 consumes the table; 09 acceptance tests zero NMT TX for each refused identity, active-work and node-0 case and one frame when admitted. 10 item 7 points to the table |
| F6 | Accepted, by the shared change | canopen_shared 02 S13 bullet and README S13 row at `a7656efb` (default deny in the S2 commit on every interface with a non-Lichuan binding; a running schedule revoked first); canopen_shared 01 (`armSync` leaves the remote hazard to the caller) | 02b header and item 3: S13's default deny lands in the S2 commit with the DS2 cases (DS2-only and mixed interfaces, the producer armed directly, admission onto an armed schedule); no DS2 binding is admitted before it lands. 09 item 7 and acceptance, 11 item 2, the execution contract owner bullet, 01 item 1 and the README stage and seam rows. Mixed interfaces still wait for the full S13 (round-1 lead decision 3) |
| F7 | Accepted | canopen_shared 02 "Registration acceptance" at `a7656efb` (the probe's read effect reviewed in the family ledger by object, access and evidence before it runs); DS2C p24 (`1000h:00` U32 RO, no PDO, 0x00040192); `[1000]` RO with default 0x40192 in DS2EDS l.119, IBLEDS l.54 and ISLEDS l.54 | 02b item 4: the probe's single-read effect review row (reviewer and date) precedes the first probe, separate from `DS-DEC-1` and S10 markers; without it the probe refuses with no TX; the device-side effect stays documented, not observed. 02b acceptance (also: no identity-dependent route is callable at 02b, from the same section). 11 item 5: the scan uses the same row |
| F8 | Accepted | 05 items 1-3 and 8 (the `pul` scale inference, `6081` pul/s versus rpm with defaults 10000 and 60, the ramp unit, the undocumented relative basis); review DS-D16, stage 01's DS-D33 and DS-D34; 12a Q8a, Q8b and Q10 bound wording | 12a item 4: a bounded-move rule for every question that commands motion (Q4 to Q10) and for 12's regression: the bound holds under each interpretation still consumed, from a fresh `6064` with an absolute target within the bounded displacement, without timely host service and independent of the behavior under test, otherwise NOT RUN. Q4, Q7 (`6081` bounded both as pul/s and as rpm) and Q9 (a move that ends within its bound if the stop does nothing) cite it; 12 physical items 3 and 7 cite it |

### Reviewer's unresolved questions

1. **Answered by the files at `a7656efb`.** The canopen_shared README table records no seam claimed or landed (S1-S3,
   S6-S8 and S10-S14 not landed, claimed by none; S4, S5 and S9 pending on prompts 64, 65 and 63). `PROGRESS.md`
   lists prompts 57, 63, 64, 65 and 77 as Pending and 51 as Partial. No family owns an in-flight shared edit; the
   shared texts the family sets consume were changed by the lead (`a7656efb`). S13's default deny now lands with S2,
   so no DS2 binding exists without it. Stage 01 re-records the table at the execution HEAD.
2. **Answered: none.** Every family manual prints 15 to 11 "emergency stop" only in the position-mode controlword
   table (DS2C p11, MINI p13, IBL60 p12, ISL60 p15), `605A` is absent and `6085` has no stated link (review "Other
   controlword use"); no source names a stop after 31 or 95, after a homing trigger, or in modes 3 and 4. So the
   `DS-DEC-2` rows are project decisions marked for the owner's review, a timed run without its mode's row is refused
   before TX (F3), `DS-DEC-3`'s S3 opt-in rests on CP-D10 or the deny stays, and 12a Q9 and Q10 collect fixture
   evidence. A fake server's acceptance is never evidence.

### Lead decision, 19 November 2026

**F1, identity collisions.** Rejected as policy. When identity cannot distinguish siblings the declaration selects
(prompt 57 "Admission by outcome"; engineering direction of 2026-11-17: no lock-out, the user owns the declaration).
DS2 keeps the admission, makes the result name every matching model, decodes only with the declared model's dialect
and adds the cross-sibling tests that assert exactly that (execution contract identity section, 03 item 4 and
acceptance).

### Open for the lead or owner

- **Mixed interfaces.** On an interface with a DS2 binding the full S13 gate refuses exactly what its default deny
  refuses, because DS2's SYNC and NMT effects are unknown. The lead may unblock 11's shared-interface positive tests
  at the default deny; until then round-1 decision 3 stands.
- **A node in NMT Stopped.** It answers no SDO (principles pack section 3), so it has no current identity outcome and
  prompt 57 refuses a targeted Start or reset to it; recovery is then a power cycle unless prompt 57 gains an
  exemption.
- **`DS-DEC-3`.** The S3 opt-in's every-state review rests on the CiA command reading (CP-D10), not on DS2 evidence.
  Without lead or owner acceptance DS2 keeps the default deny and an unresolved route has no best-effort word.

Checks: `python scripts/check_promptset.py` reports no error in this directory; `git diff --check` is clean for it.
Nothing was compiled; no hardware or port was used.

### Round 2 lead decisions on the open requests

1. Mixed-interface positive tests: round 1's decision 3 stays until S13's full gate lands.
2. NMT-Stopped recovery: prompt 57 now admits a targeted NMT Enter Pre-operational to a node
   whose fresh heartbeat or node-guarding state reports Stopped, under every outcome except
   CONTRADICTED (canopen_shared `audit_application.md` round 4, theme 3). Stage 09's NMT table
   consumes that exception; without a state report the node is reported stranded.
3. `DS-DEC-3`: the S3 default deny stays until the every-state review is accepted.
