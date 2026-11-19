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

## Round 2

A condensed, faithful copy of the second reviewer report; dispositions are in
[audit_application.md](audit_application.md) round 2. Line numbers refer to the files at the audited revision.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-184724-17daf9` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard ("large input (~160702 tokens)") |
| Date | 2026-11-19 18:47:24 (from the ID), 208.7 s, one attempt |
| Revision | `main` at `edcf9621a1f5f598ca77d5e9261f885b7946f601`, no uncommitted changes recorded |
| Cost | Estimated USD 0.35 (0.349538, configured pricing); 125,069 input and 9,940 output tokens (4,443 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-184724-17daf9.json` (local) |
| Location checks | 32 verified, 0 relocated, unverified or invalid |

Promptset under audit: the 19 files of this directory, including round 1's `audit.md` and
`audit_application.md` (`README.md` 124 lines, sha256 `b70c3ffc...`; `execution_contract.md` 184, `d58f4f50...`).
Context: canopen_shared `README.md`, `02_profile_integration_contract.md` and `01_existing_code_and_tooling.md`,
`ADD_DRIVE.md`, `COMMON.md`, the reference pack `README.md` and `review.md`, prompt 57 (194 lines, before
`a7656efb`); project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The revision fixes several round-1 defects: S3 defaults to deny before DS2 registration, the disputed
torque downloads are withheld by model, polling is gated, and the first generated headers ship with their package
checks. The remaining blockers are in admission and failure handling: ambiguous identity can still authorize
model-dependent writes, motion can lack a known controlword predecessor or a dispatchable stop, and network commands
have gaps at the owner boundary. Evidence intake and independent host work can proceed; the affected changing routes
and physical tests should stay gated. **Verdict:** blocking issues. Several acceptance criteria can be met by a
synthetic server without proving that admission rejects a wrong-model binding, that a trigger starts from a known
predecessor, or that a finite run can dispatch its planned stop under allowed owner timing.

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *A colliding identity match admits work that depends on the exact DS2 model.* The shared contract requires ambiguous identity to refuse dependent work, but the DS2 table admits changing requests on UNCONFIRMED with a declared-model match; IBL60 and IBL80 share one EDS identity with different alarm and bit-10 meanings, so a wrong-sibling binding passes and uses the wrong dialect. Keep prompt 57's comparison; split operations established across all matching candidates from those needing evidence of the declared model, and refuse the latter before TX; add real-owner cross-sibling tests (IBL60 versus IBL80, a wrong dialect reading the report as alarm-free). | execution_contract:124; contract 02:72-74; review:169-171; prompt 57:88-89 |
| F2 | High, verified defect, high | *Position and homing require a confirmed trigger predecessor but do not define how to establish it.* 04 may return already enabled on a fresh 111 with no word sent, which shows neither the previous controlword nor bit 4; the ledger needs no controlword readback; nothing covers bit 4 unknown (attachment to an enabled drive, an uncertain trigger, a second homing). Define one DS2 predecessor rule: which current-session evidence establishes the word, what retires it, which reviewed transitions re-establish it; refuse when unknown; never use 111 as proof or send 15 as an unconditional repair; test attachment, consecutive homing, uncertain trigger or rearm, and stop, reset and rebind with exact owner traffic. | 04:23-24; 05:45-47; 07:18-19; 02:28-29; AGENTS.md:79 |
| F3 | High, verified defect, high | *Finite velocity and torque lack a fail-closed gate for their required stop.* 04's only documented arrow is 15 to 11 in position mode and leaves modes 3, 4 and other predecessors to `DS-DEC-2`, yet 06 and 06b require a quick stop at expiry without saying what happens if no row is established; 04 sets the S3 shape per binding without limiting eligibility to reviewed modes and predecessors, and S3 validates a transfer shape, not the word's suitability. Make a reviewed stop policy a prerequisite of the finite start, refuse before changing TX when the mode or predecessor is out of scope, define S3 eligibility and reporting; test missing and refused stop rows and an uncertain start on an unresolved route, never clearing the gate with a synthetic drive. | 04:42-43, 49-51; 06:31-33; 06b:48-49; contract 02:226-227 |
| F4 | High, probable defect, high | *The finite-run budget does not bound the owner's wait before stop dispatch.* The budget reserves duration plus a caller stop budget from the start word, but an in-flight request settles first, so a start SDO or observation in flight until the deadline can block the stop at expiry; no exact-cutoff outcome is defined. State the scheduling assumptions and enforce transfer cutoffs so an admitted run keeps a stop dispatch opportunity strictly before expiry; keep possibly-sent and needs-stop reporting; never extend the deadline; test a start or observation in flight across expiry, the exact cutoff and the torque case through the real owner. | 06:28-30; 04:46; contract 02:107-109; 06b:46-48 |
| F5 | High, verified defect, high | *Explicit NMT requests are not covered by DS2's changing-request admission criteria.* 09 exposes start, stop, pre-operational and reset despite unknown effects; the admission matrix and the foreign-server tests omit 09 and NMT; prompt 57 refuses every changing request on CONTRADICTED except the drive stop; S13 covers broadcasts only. Add an NMT command-by-command disposition to the admission table, apply prompt 57 to targeted NMT, keep S13 for broadcasts, never treat NMT Stop as the stop exemption, define state and pending-work conditions, and test zero NMT TX on contradicted or stale bindings and each disallowed active-motion case; keep unevidenced commands gated. | 09:25-28; execution_contract:131-132; prompt 57:99-101; contract 02:211-212 |
| F6 | High, probable defect, high | *A dedicated pre-S13 interface does not itself exclude owner-produced SYNC.* The mixed-bus fix keeps DS2 off other families' interfaces, but a DS2-only interface can still have the owner's SYNC producer, which leaves the remote hazard to the caller. Specify and test that a DS2-only production configuration exposes or retains no active SYNC producer, or block that deployment until S13; no DS2-local producer or guard; include the DS2-only topology and a transition onto an already scheduled owner in the shared producer tests. | 09:34-39; canopen_shared 01:48; contract 02:234-236 |
| F7 | Medium, verified defect, high | *The registration probe is mandatory before its read effect is reviewed or expressly accepted.* The shared deliverable needs a reviewed or accepted probe read effect; 02b uploads `1000:00` with the effect unknown and no acceptance step, which also affects the scan. Give 02b a single-read effect review or acceptance path for `1000:00`, separate from polling and markers; refuse without TX when it is absent; keep unresolved effect evidence in the result. | 02b:34-36; contract 02:277-279 |
| F8 | High, probable defect, high | *Early position and stop qualification can rely on the behavior under test for its bound.* The independent-bound rule covers negative encodings, velocity, torque and homing, not Q4, Q7, Q9 or 12's small-move regression; a small nonnegative absolute target need not be a small displacement, Q9 tests a stop with unknown reaction and Q7 an unresolved rate unit. Apply the protective-bound rule to Q4-Q7, Q9 and every regression consuming unqualified facts: fresh start position, travel and speed bounded under the competing interpretations, independent of the action under test, otherwise NOT RUN. | 12a:38, 41, 44-45; 12:52-53; AGENTS.md:83 |

### Unresolved questions (reviewer)

1. Which shared seams and main-sequence artifacts are landed and tested at the execution HEAD, and which family owns
   each in-flight shared edit? (Current S4, S5 and S13 status decides which console, owner and mixed-interface
   deliverables can be accepted without parallel implementations.)
2. What model- and mode-scoped evidence establishes a usable stop predecessor and reaction after a position or homing
   trigger, or during velocity and torque? (The review gives a position-mode example only; a ledger decision or a
   fake-server result cannot supply device evidence.)

### Coverage gaps (reviewer)

Rendered vendor originals, EDS bodies, the manifest and DS2 fixture captures were not supplied; widths, units,
applicability and device reactions were not checked against primary evidence. The repository index was truncated:
no owner, SDO or SYNC implementation, package or checker code, Git state or progress records. The parent Projects
instructions, the communication practice, the conditional-experiment prerequisites, the Lichuan C extensions contract
and several consumed main-sequence prompts were not supplied. The audit was text-only: no verifier, build, CI job or
hardware test ran.

## Round 3

A condensed, faithful copy of the third reviewer report; dispositions are in
[audit_application.md](audit_application.md) round 3. Line numbers refer to the files at the audited revision.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-192232-3e8d46` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard ("large input (~177504 tokens)") |
| Date | 2026-11-19 19:22:32 (from the ID), 190.1 s, one attempt |
| Revision | `main` at `6ff80ed1dd19026da4e970220fb947437abf87ba`; uncommitted changes recorded, none in this directory (the audited hashes equal the committed files) |
| Cost | Estimated USD 0.37 (0.374068, configured pricing); 136,419 input and 10,123 output tokens (5,506 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-192232-3e8d46.json` (local) |
| Location checks | 24 verified, 0 relocated, unverified or invalid |

Promptset under audit: the 19 files of this directory, including rounds 1 and 2 of `audit.md` and
`audit_application.md` (`README.md` 124 lines, sha256 `4424453d...`; `execution_contract.md` 205, `97df0f09...`).
Context: canopen_shared `README.md`, `02_profile_integration_contract.md` and `01_existing_code_and_tooling.md`,
`ADD_DRIVE.md`, `COMMON.md`, the reference pack `README.md` and `review.md`, prompt 57 (214 lines); project
instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The stages incorporate several important earlier fixes: the S3 and S13 default denies precede DS2
binding admission, disputed torque downloads stay withheld, and identity collisions follow the declared model as
prompt 57 directs. Two newer shared rules did not reach the operative DS2 stages: a finite run needs a usable stop
path after a lost start reply, and a node in NMT Stopped needs the narrow Enter Pre-operational exception. Homing
also has conflicting instructions about who establishes its trigger predecessor. Evidence intake, tooling and
independent read-only work can proceed; the affected changing routes and qualification cases are not ready as
written. **Verdict:** blocking issues. An implementer following the acceptance criteria could admit a timed run with
no dispatchable stop after a lost start reply, would have to reject the recovery command prompt 57 admits, and could
not satisfy both homing's setup sequence and its no-TX refusal without a predecessor. These are specification
defects, not findings about firmware or observed drive behavior.

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *Timed-run admission checks stop rows but not whether a stop can leave after a lost start reply.* The shared rule separates a reviewed stop row from a usable stop path; with S10 `NONE` a lost start reply leaves the route unresolved. 06 requires `DS-DEC-2` rows, then specifies an unresolved-route case with no best-effort TX and `needsStop` kept, without requiring an independent stop; 06b inherits it. Consume the shared stop-path gate in both preparations before any changing TX, mode select included: the binding's reviewed S3 opt-in or a declared independent stop named in the result. Test that valid rows alone give zero changing TX without either path and that each admitted path stays available after a lost start reply; keep stop dispatch distinct from standstill. | contract 02:131-136; 06:37-43, 77-78; 06b:47-51 |
| F2 | High, verified defect, high | *The NMT table and tests omit prompt 57's Stopped-node exception.* Prompt 57 admits a targeted Enter Pre-operational on a fresh Stopped report under every outcome except CONTRADICTED; the round-2 application says it was consumed, but the execution table still gives that command the ordinary released-report and identity conditions, and 09 demands zero NMT TX on no outcome, unmatched or unread fields. The table's "lost until a Start" does not help, because Start has the ordinary gates. Reference prompt 57's bootstrap branch in the table and 09; use a fresh Stopped heartbeat, add no node guarding and require no SDO-derived report; test one frame per permitted outcome and zero on CONTRADICTED or without a fresh Stopped report; keep the ordinary gates and S13's broadcast denial. | prompt 57:119-121; execution_contract:147-150; 09:77-79; audit_application:144-147 |
| F3 | High, verified defect, high | *Homing both establishes and requires the enable predecessor.* 07 selects mode 6 and enables before writing parameters, while 04 says homing never writes 0x000F to make its predecessor and refuses before TX without one; a mode write also invalidates a predecessor and mode select needs a released report. Make mode select and enable explicit prerequisite operations outside `HomeContext`; `prepareHome` consumes the acknowledged 0x000F and a fresh enabled mode-6 report under the same reservation and generations, then writes the parameters and the trigger. Test the full composition and that homing alone emits no setup or trigger frame without the predecessor. | 07:18-20; 04:74-78, 81-82 |
| F4 | High, probable defect, high | *The stop review treats "DS2 never sets Halt" as evidence about the drive's halt state.* The shared contract requires the stop review to address a set halt bit and a stop row to handle unknown halt; 04 answers that the profile never sets bit 8 while bit 8 is undocumented and the S3 attempt establishes no state. A drive keeping state from traffic outside the current reservation is not excluded by the profile's own write history, and CP-D10 does not show that clearing the bit in 0x000B cannot release a latch. In the stop table and the S3 review distinguish evidenced clear or inapplicable halt from unknown or set; document the evidence per class, otherwise refuse the row and keep S3's default deny; test unknown predecessor state and attachment without trustworthy history; a fake server's acceptance is no evidence. | 04:55-60; contract 02:144-148, 259-265; AGENTS.md:79 |
| F5 | High, probable defect, high | *Mixed-bus admission lacks a safe way to obtain its live identifier evidence.* 11 requires live COB-IDs and overlap checks before sharing; 09 reads them by SDO over a route that assumes default identifiers while the EDS claims configurable channel identifiers. No path verifies the identifiers before using a possibly conflicting route or says when isolated readback carries into the shared domain. Separate isolated collection from admission; name the verified SDO bootstrap route and the complete identifier set and how it stays applicable; keep sharing blocked when the route is unknown or the evidence cannot carry; test non-default SDO identifiers, unavailable or stale readback, and service-route as well as PDO-to-PDO collisions. | 11:14-16; 09:54-55; contract 02:176-183 |
| F6 | Medium, verified defect, high | *The scan cannot produce its promised identity outcome from a node list alone.* 11 scans caller-listed nodes and promises identity reads and an outcome without selecting a model, but 03 needs the declared model to choose sourced sub-indexes and prompt 57 needs it for the outcome; 11 does not say whether entries resolve to declarations. Define probe-only for an address without a declaration and 03's identity plan for a declared binding, or require a declaration per identity entry; test unbound addresses, MINI with an oversized count and bound siblings with ambiguous identity; never select a model or read outside the declared scope. | 11:37-40; 03:10-13; prompt 57:79-82 |
| F7 | Medium, verified defect, high | *The physical bound requires an absolute target even for runs that have none.* 12a applies the bounded-move rule to Q4 through Q10, but the rule requires absolute targets and a displacement from fresh `6064`; Q8b and Q10 include velocity, torque and homing, and 12 asks for relative qualification under the same rule. Keep a universal bound independent of host service and of the behavior under test; put the fresh-position and absolute-displacement checks in a position branch; make relative cases conditional on a verified basis and a separately justified bound; point velocity, torque and homing to their own bounds; keep NOT RUN without the evidence. | 12a:59-64, 45; 12:41-43 |

### Unresolved questions (reviewer)

1. Which shared seams and main-sequence prerequisites have landed and passed at the execution HEAD, and who owns any
   in-flight shared edits? (S4, S5 and full S13 decide which console, package-component and mixed-interface
   deliverables can be accepted without parallel implementations.)
2. What stop rows and stop paths can be justified for homing after its trigger, including when no completion arrives
   and the SDO route is unresolved? (07 records a stop obligation on timeout, but no evidence documents a quick-stop
   reaction after the homing trigger; the interrupt is explicitly not a stop, so it cannot be the fallback.)

### Coverage gaps (reviewer)

The DS2 rendered originals, EDS bodies, product-page snapshots and manifest were not supplied; stages were compared
with the reference review only. The repository index was truncated: owner, SDO, SYNC, package, checker and workflow
code were not supplied, so landed seam state and verification commands could not be confirmed. The parent Projects
instructions, the communication practice, the conditional-experiment prerequisites and several consumed main-sequence
prompts were not supplied. No DS2 fixture record or raw capture existed. The audit was text-only: no build, test, CI
run or hardware behavior was verified.

## Round 4

A condensed, faithful copy of the fourth reviewer report; dispositions are in
[audit_application.md](audit_application.md) round 4. Line numbers refer to the files at the audited revision.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-195034-e90304` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard ("large input (~191973 tokens)") |
| Date | 2026-11-19 19:50:34 (from the ID), 198.7 s, one attempt |
| Revision | `main` at `f864cbb9a1f6f24876bb13d4911db20bd90a5454`; uncommitted changes recorded, none in this directory (the audited hashes equal the committed files) |
| Cost | Estimated USD 0.40 (0.402052, configured pricing); 146,281 input and 10,949 output tokens (6,017 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-195034-e90304.json` (local) |
| Location checks | 28 verified, 0 relocated, unverified or invalid |

Promptset under audit: the 19 files of this directory, including rounds 1 to 3 of `audit.md` and
`audit_application.md` (`README.md` 124 lines, sha256 `0da462f8...`; `execution_contract.md` 209, `201aca8e...`).
Context: canopen_shared `README.md`, `02_profile_integration_contract.md` (346 lines, before `c138e959`) and
`01_existing_code_and_tooling.md`, `ADD_DRIVE.md`, `COMMON.md`, the reference pack `README.md` and `review.md`, prompt
57 (217 lines); project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The stages incorporate the earlier fixes for default-deny registration, NMT Stopped recovery and homing's
separate enable prerequisite. The remaining blockers are in stop handling: an acknowledged stop can make the next stop
wrongly clear Halt, and a timed run can pass admission without a usable stop while its SDO route is still resolved.
Evidence intake, tooling and independent read-only work can proceed; the affected changing routes and physical
qualification cases stay gated until their failure paths are specified and tested. **Verdict:** blocking issues. The
stop-history contradiction is explicit in 04; the timed-run rules do not cover a resolved route whose drive report
fails normal stop admission; the network stages leave queued NMT dispatch and mixed-bus read-back dependent on unstated
assumptions. These are promptset findings, not observations about firmware or hardware.

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *An acknowledged stop that sets Halt can be recorded as clearing it.* 04 makes an acknowledged whole-word download enough for the halt-clear class without checking bit 8, and says a stop does not end the class. On an unknown attachment the stop sends 0x010B, which can be acknowledged and then meets the clear criterion, so the next stop sends 0x000B and may clear a set Halt, against the shared rule; the same fails when S3 possibly sends 0x010B after a clear word. Track halt evidence from the actual word and its transmission outcome: only an acknowledged bit-8-clear word with no later possibly sent bit-8-set word gives clear; keep the trigger predecessor separate. Test two successive stops from an unknown attachment and a possibly sent S3 stop after a clear word. | 04:18-20, 20-23, 72-74; contract 02:146-147 |
| F2 | High, verified defect, high | *Timed-run admission can leave no stop path on a resolved SDO route.* 06, 06b and 07 accept an opted-in S3 shape as the stop path, but S3 runs only on an unresolved route and 04 refuses a normal stop on a stale, missing or alarmed report. A run with acknowledged start writes can lack fresh state at stop due: the route stays resolved, the normal stop refuses, S3 is ineligible and no independent stop was required. The transfer cap does not close this. Require a dispatchable stop for resolved and unresolved routes after any possibly effective start; where ordinary admission can fail and S3 cannot run, require the declared independent stop unless a reviewed route covers the case. Real-owner cases: an acknowledged start followed by missing or stale state at stop due and by a fresh alarmed report; assert the available path, keep `needsStop`, infer no standstill from dispatch. | 04:12-14, 77-78; 06:41-44; 06b:51-52; 07:61-63 |
| F3 | High, verified defect, high | *Velocity and torque prescribe an enable script outside the source-state table.* 04 leaves standalone enable in modes 3 and 4 unsupported, but 06 and 06b consume only the initial `released` predicate and then prescribe 6, 7, 15 with no source-state rows or intermediate reports, so the final start word can follow an acknowledged word whose expected state was not reached; write preflight does not validate transitions. Define rows for the embedded mode-3 and mode-4 enable steps without exposing standalone enable: the first word, the evidence before each later word, and the handling of an acknowledged or uncertain intermediate word without it. Test exact traffic from every admitted starting report; an unexpected intermediate report never yields the start word. | 04:38-40; 06:9-13; 06b:14-18; contract 02:141-146; AGENTS.md:79 |
| F4 | High, probable defect, medium | *NMT admission rules do not cover commands waiting to transmit.* S13's cases revoke a running SYNC schedule when DS2 is bound and refuse new node-0 NMT requests, but say nothing of a broadcast accepted before the binding change; 09 tests targeted NMT against already pending work, not the reverse order. If admission and transmission are separate service steps, a queued Start, Stop or Reset could leave after DS2 is bound or after a conflicting operation or state change. Specify the owner-side order: retire unsent broadcasts before a DS2 binding becomes active, reserve or revalidate a targeted command before hand-off, and distinguish a handed-off frame from a cancellable one. Test both orders through the real owner. | 02b:34-38; 09:27-30, 84-86; contract 02:276-282 |
| F5 | High, probable defect, high | *Mixed-bus read-back detects identifier changes only after the bus is joined.* 11 checks each predefined SDO route against the other nodes' isolated records and then sends the read-back on the shared bus without establishing that the records still apply. A node restarted or reconfigured while being moved can make the first read-back use an identifier another node receives as an RPDO (an RPDO carrying `6040` or `60FF` is a motion command); refusing later traffic cannot recall that frame, and S13 does not cover this SDO-to-PDO collision. Define what lets isolated evidence carry into the shared topology before any shared-bus request (an evidenced unchanged session and configuration, or a separately established quiescent attachment procedure); otherwise keep the nodes isolated. Test a route change between capture and attachment: no potentially conflicting bootstrap frame is sent. | 11:22-25, 26-28; 09:22-23 |
| F6 | High, probable defect, high | *The homing qualification rule lists an unverified index as a protective bound.* The general rule requires a bound independent of host service and of the behavior under test, yet the mode-specific rule offers travel to a declared index, while Q10 may be investigating whether the encoder provides an index at all. A declaration satisfies the method's input check but does not bound a search when the pulse or the drive's response to it is the question. Separate method prerequisites from fixture protection: the plan bounds the run if the index never appears or the drive does not stop on it; apply the same to any switch whose detection or stopping effect is under test; otherwise NOT RUN. | 12a:45, 60-62, 67-69; 07:43-44 |
| F7 | Medium, probable defect, high | *Restore records do not distinguish the same profile and node on different interfaces.* 10 keys restore records by profile and node, although the bus is part of the address and separate interfaces are the default. Two DS2 bindings at one node ID on different interfaces are indistinguishable, the key survives replacement of the device at that address, and identity admission alone does not tie a snapshot to its binding when product references collide; the wrong record could change gear or heartbeat settings on the wrong drive. Bind each snapshot to interface, node, resolved profile and model, and the binding and configuration evidence of its capture; reject a different target or stale evidence; define the revalidation for a planned restore after restart; test same-node bindings on separate interfaces, rebinding and a partially applied settings write. | 10:42-44; 08:20-22; contract 02:181-184 |

### Unresolved questions (reviewer)

1. Which shared seams and main-sequence artifacts are landed and verified at the execution HEAD, especially S4, S5,
   full S13 and prompt 57's shared comparison? (They decide whether console, component packaging and mixed-interface
   acceptance can run without a parallel path.)
2. Which model- and mode-scoped stop reactions can actually be established after a trigger or partial enable, and
   under which fixture conditions? (A reviewed command shape and a synthetic acknowledgement test host admission but
   establish neither the device's stop reaction nor physical standstill.)

### Coverage gaps (reviewer)

The DS2 rendered originals, EDS bodies, product-page snapshots and manifest were not supplied; device facts were
compared with the reference review only. The repository index was truncated: owner, SDO and NMT implementation
bodies, package and checker sources, workflows and Git state were not supplied, so queue behavior, landed seams and
build gates could not be verified. The parent Projects instructions, the full conditional-experiment prerequisites and
several consumed main-sequence prompts were not supplied. The audit was text-only: no script, build, CI job or hardware
test ran, and no DS2 fixture capture existed.
