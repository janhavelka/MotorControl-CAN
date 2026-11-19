# Independent audit of the Lichuan DS2-C promptset, 19 November 2026

A condensed, faithful copy of the reviewer's report. It is untrusted reviewer input; the
verified dispositions are in [audit_application.md](audit_application.md). Line numbers
refer to the files at the audited revision, before the revision that applied the findings.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-180844-aac472` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-astra` requested and used, reasoning effort xhigh, depth complex ("focus mentions safety-critical control") |
| Date | 2026-11-19 18:08:44 (from the ID), 292.2 s, one attempt |
| Revision | `main` at `1043608bd0f58c5b371877424d2c783f9ed33e72`, no uncommitted changes recorded |
| Cost | Estimated USD 1.48 (1.484168, configured pricing); 111,766 input and 14,751 output tokens (5,685 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-180844-aac472.json` (local) |
| Location checks | 68 verified, 0 relocated, unverified or invalid |

Promptset under audit: the 17 files of this directory (`README.md` 122 lines, sha256 `4649f94a...`;
`execution_contract.md` 167, `6692017d...`; stages 01 to 12a). Requirements: canopen_shared `README.md`
and `02_profile_integration_contract.md`, `ADD_DRIVE.md`, `COMMON.md`. Context: the reference pack
`README.md` and `review.md`, prompt 57 (185 lines, before `0ab4e181`), canopen_shared
`01_existing_code_and_tooling.md`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The set makes several sound safety distinctions: binding is a caller declaration, the
status dialect is model-specific, enabling in velocity or torque mode is a start, PDO defaults are not
live maps, and neither fault reset nor persistence is borrowed from the stepper profile. It is not ready
for independent execution as written: registration omits a required owner-level stop guard, the identity
table permits motion on a positively contradicted product, and the torque and state-transition stages
require changing operations their evidence does not fully admit. Several acceptance and first-contact
cases lack prerequisites or a valid test oracle. **Verdict:** blocking issues. Documentary inventory and
much read-only work can proceed; registration must not admit a DS2 binding before the shared default-deny
guard exists, and motion must not ship with an identity override, a guessed download width or a universal
enable script.

## Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | Critical, verified defect, high | *02b admits the first DS2 binding before S3's mandatory default deny.* 02b defers all of S3 because DS2 has no stop API yet, but the owner already accepts the Lichuan stop word on every channel; the shared contract puts the deny in the S2 commit and only the family opt-in in the stop stage. Require the deny in the S2 commit; test `admitBestEffortStop` on unconfigured DS2, unresolved and rebound channels, also after a timeout (`INVALID`, no TX, Lichuan unchanged); leave the DS2 shape to 04; update both seam tables. | 02b:6-7; canopen_shared 01:49; contract 02:220-223 |
| F2 | Critical, verified defect, high | *A caller acceptance can turn a known foreign product into an admitted motion target.* The table allows standard-object operations on CONTRADICTED after acceptance, including the foreign 0x1000/0x0001; DS2 enable and motion use standard objects. Prompt 57 refuses motion on a differing product and now defines the comparison, while the DS2 contract names the extensions contract as the definition; 03 reports a differing `1000` without saying how it affects motion. Make prompt 57 authoritative with one operation-by-outcome table; keep bootstrap reads and firmware differences non-blocking; no acceptance authorizes enable or motion on a positive contradiction; specify a differing `1000`, unread fields and stale observations; test a correctly labelled DS2 binding on a foreign fake server with acceptance set through each motion preparation: zero changing TX. | execution_contract:101-103, 115; 03:18-19; prompt 57:71-74, 79-81 |
| F3 | High, verified defect, high | *Restricting the value of `607F` does not resolve its download width.* 06b writes `607F` with 1..65535 while U16 and U32 claims stand; a two-byte and a four-byte download are different requests, and sending mode or target writes first violates write preflight. Make the width a per-scope write-policy gate; admit only resolved scopes, otherwise withhold the whole sequence including mode select; test the exact size on resolved rows and zero changing frames otherwise. An upload of one size or a synthetic server accepting it resolves nothing. | 06b:14-16, 24-26; review:395-396; contract 02:99-102 |
| F4 | High, verified defect, high | *The IBL `2406h` write lacks the qualification the review requires.* A model declaration, an admitted outcome and a successful read establish neither meaning nor write effect; the review withholds vendor writes pending live evidence and a qualified fixture; the contract's must-not-exist range includes `2406h` while 06b requires a write route. Separate forbidden reuse of Lichuan C definitions from the IBL read candidate; give the download its own withheld ledger policy; test readable `2406h` plus an admissible identity: zero torque setup or enable writes. | 06b:27-31; execution_contract:62; review:471-473 |
| F5 | High, verified defect, high | *Mode checks and low-bit tests stand in for source-state admission.* 04 prescribes 6/7/15 on a fresh mode 1 or 6, also from 111 (shutting the drive down first); 06 equates any non-111 report with released; neither resolves alarm, intermediate or unrecognised reports; 08 admits stationary settings changes while its acceptance requires refusal while enabled. Have 04 deliver the DS2 source-state/intent table (dialect, mode, low bits, alarm, freshness, intent: first word, already satisfied or refusal) before motion stages; keep undocumented states unknown; make 06, 06b and 08 use the same predicates; choose and test the enabled-but-stationary settings policy. | 04:14-17; 06:11-12; 08:13-14, 63-64; contract 02:115-119; AGENTS.md:79 |
| F6 | High, probable defect, high | *Position rearm is not restricted to a predecessor that still permits word 15.* "After acknowledgement or a terminal" lets a queued rearm write the enable word after a stop, cancel or uncertain trigger, against the no-restart acceptance. Enumerate eligible acknowledgements, predecessor evidence and generations in `DS-DEC-5`; retire a pending rearm on stop admission, cancel, timeout, lost reply, reset, rebind or expiry; never append word 15 as cleanup; assert traffic when a stop races a rearm and when the trigger outcome is unknown. | 05:36-38, 61-63; 04:14 |
| F7 | High, probable defect, high | *Rejecting negative inputs does not define the safe signed/unsigned intersection.* The printed endpoint 2147483648 as unsigned is 0x80000000, negative under I32; "range edges" is no oracle. Per scope, the unqualified range is the intersection with identical wire meanings, capped at 2147483647; validate the exact quantized value before narrowing; test 2147483647, 2147483648, negatives and rounding across the boundary on native and common paths with zero changing TX; define any signed range separately. | 05:26-28; 06:14-16; review:389-394 |
| F8 | High, verified defect, high | *Repeated completion observations depend on a read-effect decision no stage delivers.* 02 allows only explicit single reads pending `DS-DEC-1`; the boundary needs the decision before polling; 03 does not classify repeated reads or give a budget; 05's fall-then-rise relies on it; S10's `NONE` default does not qualify these reads. Assign an owner for `DS-DEC-1` (objects, scopes, repeat policy, budget, `selected_by`) before the first polling consumer, or leave that subset blocked; say what acknowledged-only or `NOT_OBSERVED` result exists without polling; test both cases; keep marker approval separate. | 02:39-41; execution_contract:64; 03:50-51; 05:39-42 |
| F9 | High, probable defect, high | *Finite run expiry is not reconciled with the immutable operation deadline.* A start admitted just before cutoff can leave its planned stop outside the operation, where no changing TX is allowed. Define the event the duration counts from; before the start word check with checked arithmetic that the remaining budget covers the run and the stop dispatch; test cumulative setup delay, equality at cutoff and service resumed after cutoff; distinguish a stop never sent from one possibly sent; keep the obligation; never extend the deadline or report success. | 06:25-26, 55-57; 06b:16-17; contract 02:108-109 |
| F10 | High, verified defect, high | *The first public headers and their package gates are assigned to different commits.* 02 generates installed headers but the install rule, package lists and consumers sit in 02b; S7 also needs a registered negative test for the DS2 operation checker. Move the first headers' install rule, package entries and consumer checks into 02's commit; add the DS2 checker negative test with disjoint, complete ownership; 02b extends lists only for its new files. | 02:42-44, 48-50; 02b:52-54; canopen_shared 01:111-114; AGENTS.md:25 |
| F11 | High, verified defect, high | *Stage 03 requires identity outcomes and EDS variants its references cannot produce.* With every DS2 model a candidate, the manual pair matches all models, the DS2/ISL EDS pair several and the IBL pair two, so no documented pair gives CONFIRMED; MINI has no EDS. An announced count of four requests `1018:04` even where no sub04 is sourced; a short count is unspecified. Separate synthetic truth-table tests from production claim-set tests asserting the actual matches and UNCONFIRMED outcomes; manual-only MINI fixtures; specimen references only for qualified observations; derive sub-index reads from live count and scope membership; test short, oversized and contradictory counts. | 03:10-11, 76-77; execution_contract:105-109; prompt 57:84-85 |
| F12 | Medium, verified defect, high | *The required first-contact snapshot is not available after 03.* Q2 includes PDO records, `1014` and `1017`, whose routes belong to 09, and the console path may wait on S4. Split Q2 into an identity-admitted 03 snapshot and a 09 network snapshot, each with its route and prerequisite; say when an installed API suffices versus S4; record unavailable subsets NOT RUN; no raw read path. | 12a:6-7, 24, 40; 09:54-55; execution_contract:40-42 |
| F13 | High, verified defect, high | *The ramp-unit experiment needs a write production admits only after a unit declaration.* Q6 varies `6083`/`6084`, but `VERIFIED_CONFIGURED` writes nothing and `WRITE_NATIVE` needs the declared dimension. List manufacturer clarification, applicable evidence or a separately assigned reviewed experiment (snapshot, permitted changes, observation, restoration) as Q6's paths, otherwise NOT RUN; no provisional production declaration as the result. | 05:21-23; 12a:28; contract 02:198-200 |
| F14 | High, probable defect, high | *The proposed physical bounds do not cover a misinterpreted target or host loss.* Q8 offers active `607D` limits as a bound for both negative position and negative velocity, but a travel limit bounds no speed in mode 3; Q10 bounds torque with the very speed limit whose clamping it asks about, plus a host stop. Split DS-D09 and DS-D10 qualification; require a reviewed bound effective for the actual mode and worst-case reading, with speed and load consequences, independent of host service; never count the clamp or stop under investigation; otherwise NOT RUN. | 12a:30, 32; 06b:36-38; review:260 |
| F15 | High, verified defect, high | *The first-power checklist cites a single supply range despite a model-specific conflict.* It gives 24..70 VDC from DS2C for any fixture, while the review records 24..50 V versus 24..70 V for IBL60. Use a per-model, suffix and nameplate electrical check; require a documented safe choice or manufacturer clarification before power, never the higher limit; include the input interface, output load and brake versus braking-resistor terminals in the same sign-off. | 12a:11-12; review:435-441; AGENTS.md:71 |
| F16 | High, probable defect, high | *The pre-S13 mixed-bus exclusion is recorded, not enforced at the shared producer.* Refusing `prepareSync` for a DS2 target does not stop SYNC requested for another family or already armed; prompt 51's producer refuses only on a foreign producer; the same-domain analysis also excludes NMT broadcasts. Make positive shared-interface admission conditional on an enforceable no-SYNC/no-broadcast configuration, or keep it blocked with separate interfaces; test both families' requests and DS2 attached while a schedule is armed; after S13, consume its producer tests, no profile-local producer. | 09:35-37; 11:20-22; canopen_shared README:64; contract 02:233-235 |

## Unresolved questions (reviewer)

1. Which shared seams and main-sequence artifacts have landed, with which tested interfaces, at the
   execution HEAD? (S4, S5 and S13 decide which console, package and mixed-interface deliverables can
   be accepted.)
2. What model-scoped source permits a MINI `607D:02` read, given that MINI has no EDS and its manual
   prints both limit rows as `:01`?
3. How will the lossless ledger checker account for the second part of DS-D30, the `P02-34` versus
   `P0B-34` monitor designation, when the stage's conflict list labels DS-D30 only as `60FD`?
4. What are the bounded overflow and evidence-loss semantics for retaining EMCY events under queue or
   console pressure, given that 09 asks to retain every EMCY?

## Coverage gaps (reviewer)

The original rendered manuals, EDS contents and `SOURCES.json` blobs were not supplied (DS-D33 and
DS-D34 were taken as stated intake corrections). The repository index was truncated: no C++ sources,
CMake or package lists, checker scripts, workflows, Git state or progress records. The Lichuan C
extensions contract, the parent Projects instructions, the communication practice, the conditional
experiment prerequisites, several main-sequence prompts and sibling seams were not provided. No DS2
fixture record or raw traffic existed. The audit was text-only: no checker, build, CI job or hardware
test ran.
