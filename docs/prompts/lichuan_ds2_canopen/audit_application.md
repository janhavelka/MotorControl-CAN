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
