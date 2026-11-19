# Independent audit of the MOONS' CANopen promptset, 19 November 2026

A condensed, faithful copy of the reviewer's report. It is untrusted reviewer input; the
verified dispositions are in [audit_application.md](audit_application.md). Line numbers
refer to the files at the audited revision, before the revision that applied the findings.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-175226-dd78d8` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-astra` requested and used, reasoning effort xhigh, depth complex (focus on timing and safety-critical control) |
| Date | 2026-11-19 17:52:26 (from the ID), 308.4 s, one attempt |
| Revision | `main` at `eb3dae18c7933ae0dbb887c1415246d06812d45f`, no uncommitted changes recorded; the family files' SHA-256 equal their blobs at `84eaf687` and `1043608b` |
| Cost | Estimated USD 1.49 (1.488328, configured pricing); 113,096 input and 14,589 output tokens (5,871 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-175226-dd78d8.json` (local) |
| Location checks | 66 verified, 0 relocated, unverified or invalid |

Promptset under audit: this directory's `README.md`, `execution_contract.md` and stages 01,
02, 02b, 03-14 and 14a. Requirements: `canopen_shared/README.md` and
`02_profile_integration_contract.md` (before round 2), `ADD_DRIVE.md`, `COMMON.md`. Context:
the pack `README.md` and `review.md`, `docs/audit/2026-11-19_family_selection.md`,
`canopen_shared/01_existing_code_and_tooling.md`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The family split, the read-only probe, the separation of documented behavior
from hardware qualification and the ban on the CL86-C bench are sound. The set is not ready
for independent execution: some stages require writes the ledger must withhold, the SYNC
acceptance test can pass without accounting for every broadcast effect, registration must
both accept and reject the same Lichuan value, and several first-contact questions need
operations the ordinary API must refuse until those questions are answered. **Verdict:**
blocking issues. Evidence intake, read-only work and independently gated subsets can
proceed; reconcile the write policy, binding migration, SYNC and watchdog admission and the
qualification paths before dispatching the affected stages. A fake drive that accepts a
chosen width, action code or state sequence makes a choice reproducible, not correct.
Nothing was built, run or tested.

## Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | Critical, verified defect, high | *The SYNC acceptance test can pass while a watchdog feed or synchronous PDO effect is unknown.* Stage 09 names three SYNC effects, but its positive case admits production after one fresh non -2 mode read, which says nothing about a pending synchronous RPDO or a watchdog fed for another application; S13 refuses unknown or harmful effects on every bound device, during a running schedule too. Cover the declared bus population, mode, live PDO communication and map state and watchdog state and feed ownership per binding; refuse unknown or unauthorized effects; revoke before a potentially changing write and on invalidation, also during a schedule. Test a non -2 node with an unknown map, a pending motion RPDO, an unintended SYNC watchdog feed, and reset or rebind during a schedule, with no later SYNC TX until requalified. | 09:52-54, 83-85; shared README:63 |
| F2 | High, verified defect, high | *Later sequences require downloads that stage 02 must withhold.* SSDC torque writes 0x6087, which stage 06 itself records as INT8 WO or UINT32 RW; native units choose no width. Stage 11 offers a 0x2020 change despite its width conflict and re-arms through 0x2060 sub 2 despite conflicting access and widths; stage 09's remap needs conflicted 0x1400 writes. Preflight every download of an operation against its write policy before the first changing step, enable and partial setup included; mark the operation BLOCKED where an encoding or access is unresolved; a narrower product-specific operation only with recorded evidence resolving that conflict. A read of one candidate size or a synthetic server resolves nothing. Test zero changing TX for torque setup, node-ID change, watchdog re-arm and the affected remaps. | 02:30-32, 70-72; 06:42, 46-47; 11:20-22, 39-40; 09:39-41; review:521-523; AGENTS.md:71 |
| F3 | Critical, verified defect, high | *Knowing a Q program's contents does not qualify its watchdog action code.* Stage 11 permits action 2 and above with stage 10's segment qualification, but MO-D18 says the two published code ranges disagree and neither has 12 values: evidence for segment N is not evidence that action byte X starts N. Stage 06 says the drive keeps running on host loss, while the review says no default reaction is documented. Refuse every Q-valued action until both the action-to-segment mapping and the program content are qualified; for stop actions require resolved write policies, applicable mode and a qualified feed plan (a nominal period proves neither delivery nor a reaction); publish "motion may continue; no stop established". Test a known segment with an unknown mapping, an inapplicable mode and a feed supplied by another schedule. | 10:17-20; 11:42-44; 06:30-32; review:399-401, 547-549; AGENTS.md:77 |
| F4 | High, verified defect, high | *S2 migration requires rejecting the canonical Lichuan value.* Stage 02b refuses both 41 and 1, while S1 assigns `LICHUAN_C = 1` and S2 accepts it; an old literal 1 and the migrated enum are the same value, so obeying 02b breaks the unchanged-traffic Lichuan consumer. Reject 41, zero, unknown, cross-family values and protocol mismatches; move literal-1 callers to the enum and assert that a valid Lichuan value-1 binding still resolves with unchanged traffic. | 02b:20-24; shared 02:39-43, 55-57 |
| F5 | High, verified defect, high | *Landing S13 does not unlock the proposed mode -2 operation.* S13 refuses SYNC with a node in mode -2; stage 10 lets a later mode -2 operation own the schedule while keeping the node SYNC-unsafe, but owning a schedule is no S13 exception; stage 10's acceptance requires universal refusal while 14a suggests testing -2 after S13. Choose refusal-only for this set and defer the positive route and the Q11 subcase; a positive SYNC Q start needs a separate shared-contract change (intentional, exclusive, qualified trigger with activation, interruption, lost-mode-write and teardown tests) without weakening the ordinary gate. | 09:83-85; 10:41-45, 64-66; 14a:35 |
| F6 | High, probable defect, high | *The position admission checks do not establish that clearing halt is safe.* An enabled, fault-free mode-1 drive with bit 12 clear passes stage 05's idle test, but bit 12 is disputed buffer evidence, not proof that no paused target is pending; the bit-4-low setup word also clears halt, which stage 04 says can resume the previous target, so the wrong motion can precede the new trigger and be mistaken for it. Define idle admission apart from buffer space; require current binding-specific evidence excluding a paused or pending target and an unresolved prior start or halt before any halt-clearing word; otherwise refuse; make resuming a distinct intent. Test a halted pending move with bit 12 low, also after rebinding or loss of predecessor evidence. | 05:38-41; 04:52-56; AGENTS.md:79 |
| F7 | High, verified defect, high | *Layout confirmation, exact model evidence and firmware permission are not distinct enough.* Stage 03 confirms UM21 by vendor ID and series where the review requires series, version and vendor ID together; it says unknown firmware gates hardware-qualified claims while the execution contract gates dependent operations. For AMP10, stage 13 names FWName, software version, an absent-object abort and EDS defaults but no positive fixture-qualified model match, yet admits modes by exact model. Publish an admission table separating bootstrap reads, series and layout evidence, model evidence and the firmware or configuration each operation needs; outcomes for missing, contradictory, default-looking and stale data; no abort, "UNK" or EDS default as model evidence; keep inferred AMP10 reads labelled and model-specific Part B operations gated; synthetic qualifications in host tests, not a production bypass. | 03:12-15, 19-22; review:193-195; contract:81-82; 13:26-29, 43-45 |
| F8 | High, verified defect, high | *Some UM21 stop descriptions promise physical or restart effects beyond their evidence.* Stage 04 turns an expected Switch on disabled into "does not hold position" while leaving the brake unobserved; it states the AMP10 immediate Q halt and restart from the beginning as a per-mode effect, and stage 10 repeats it for UM21. Specify the command sent, the fresh state observed and the mechanical or program effect not observed; promote no shaft holding or release or UM21 restart point from a state transition or an AMP10 passage; test that an acknowledged halt or quick stop publishes none. | 04:31-33, 44-46; 10:27-30; review:305-308 |
| F9 | High, probable defect, high | *Homing can be admitted without knowing which physical boundary it approaches.* Stage 07 leaves the first switch approached unresolved, yet admits a switch method on an input-function read and a wiring declaration; the limit reaction is undocumented, so a host deadline cannot bound travel after communication loss. The one enabled sequence needs an exception table: the review attributes no-enable operation to method 37, the stage to 35. Per moving method require a fixture- and configuration-bound direction and approach qualification, the applicable switches or index and a reviewed stop and travel plan, otherwise withhold homing; separate stationary coordinate methods and verify their rules on the rendered UM21 page; test no TX for unknown and stale direction qualification. | 07:21-24, 27-29, 35-38, 48-50; review:285-287 |
| F10 | High, verified defect, high | *First-contact evidence is requested through routes that must reject it.* Q7 needs a second set-point while stage 05 admits one idle move; Q12 needs a 0x2020 store that stage 11 refuses; Q13 targets two bitrates that binding admission rejects. Give each question an executable path: vendor clarification, a separately reviewed and authorized isolated experiment with permitted actions, observation and restoration boundary, or passive analysis; otherwise NOT RUN with the evidence needed; no raw console bypass, no synthetic pass, no guessed store group. | 14a:6-9, 31, 36-37; 05:51-53; 11:21-23; 02b:45-46 |
| F11 | Medium, verified defect, high | *Stage 07 consumes the settings context that stage 08 has not delivered.* Stage 07 requires only 03-05; stage 08 says 07 reuses its context while 08b needs 07's rules. Deliver a small tested settings-context foundation after 03 and before 07, or make 07's homing request self-contained; update the stage table, gates and tests together. | 07:4-5; 08:4-5, 9-11 |
| F12 | High, probable defect, high | *Per-write deadlines can extend a move beyond its operation deadline.* Stage 05 makes each write immutable and tests a deadline after each write; several individually timely setup writes can consume the whole budget and still permit a trigger. Capture one absolute deadline at preparation, bound every inner deadline by it, check expiry before every changing step; test cumulative delay reaching the deadline before the trigger: no trigger TX, accurate partial-effect evidence. | 05:41-42, 65-67; shared 02:86-88 |
| F13 | Medium, verified defect, high | *Unknown-length identity strings have no bounded retention or overflow contract.* Stage 03 retains every raw string, admits segmented replies and calls three lengths unknown; fixed caller storage cannot promise complete retention, and a segmented initiate is not a completed value. Define capacities and bounded transfer behavior in stage 03; report complete, incomplete, oversized and aborted fields distinctly; never confirm identity from a prefix; test exact fit, one byte over, missing length, interrupted segments and preserved other fields. | 03:10-11, 23-25; AGENTS.md:57, 63 |
| F14 | Medium, verified defect, high | *Shared verifier registration is assigned before the S7 claim.* Stage 01 registers the checkers in shared verifier, repository and CTest lists, which S7 covers, while the claim sits with the first public header in stage 02. Claim or extend S7 before stage 01's registration, recording the partition work left for stage 02, or keep 01's checks family-local; no first header without the complete partition. | 01:60-64; 02:47-49; shared README:27-30, 57 |

## Unresolved questions (reviewer)

1. Which real identity-dependent preparation is available for stage 02b's stale or
   colliding-identity refusal test, before stage 03 delivers identity and config or state
   operations? A test that sends nothing because no route exists proves no guard.
2. At the executing revision, which owner-admitted path can produce the reviewed,
   non-retriggering RPDO feed first-contact Q9 needs? Without a tested producer the RPDO-fed
   experiment stays blocked.
3. Which stage owns an executable common angle-move case or an explicit unsupported
   disposition, with reference, scale and angle-path policy? Stage 01 counts it; no stage
   specifies it.
4. Which pinned, available revision supplies the CANopen principles evidence to a fresh
   executor? The README calls the pack present in the working tree but not committed.

## Coverage gaps (reviewer)

The reference README and review are secondary evidence; the original PDFs, rendered figures,
EDS bytes, extracts and `SOURCES.json` were not supplied, so MO-D31 to MO-D33, the 0x1011
access correction and their hashes need verification at intake. The repository index was
truncated: current headers, owner and service code, adapter timing table, generators,
checkers, package lists, workflows, seam landings and main-sequence progress were not
checked. Parent instructions, the convention documents, main-sequence prompts and current
MotorControl-RS and FieldCore-node sources were not supplied. No MOONS' fixture, firmware,
timing, electrical or restoration evidence or live authorization existed; no checks ran; the
CL86-C bench artifacts qualify no MOONS' drive.

## Round 2

A second audit of the revised set, condensed the same way. Line numbers refer to the files at
the audited revision, before round 2 was applied.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-184315-2fffe2` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard (large input, about 170,842 tokens) |
| Date | 2026-11-19 18:43:15 (from the ID), 230.9 s, one attempt |
| Revision | `main` at `edcf9621a1f5f598ca77d5e9261f885b7946f601`, no uncommitted changes recorded; the family files' SHA-256 equal their blobs at `edcf9621` and `a7656efb` |
| Cost | Estimated USD 0.38 (0.376326, configured pricing); 133,483 input and 10,936 output tokens (4,514 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-184315-2fffe2.json` (local) |
| Location checks | 34 verified, 0 relocated, unverified or invalid |

Promptset under audit: this directory's 20 files (README, execution contract, stages, round 1
audit and application). Context: `canopen_shared/README.md`, `02_profile_integration_contract.md`
and `01_existing_code_and_tooling.md`, prompt 57, `ADD_DRIVE.md`, `COMMON.md`, the pack `README.md`
and `review.md`; project instructions `AGENTS.md`, `CLAUDE.md`. The shared contract and prompt 57
were read as they stood before `a7656efb`.

**Summary.** Round 1 fixes hold: conflicted writes are withheld, Lichuan value-1 bindings stay
valid and SYNC-triggered Q starts are refusal-only. Implementation-blocking gaps remain in
broadcast admission, stop handling, finite-run timing and first-contact evidence paths.
**Verdict:** blocking issues. Evidence intake, the read-only probe and independently gated work
can proceed; the affected motion, network and bench paths are not ready. These are
specification defects; no code or hardware was examined.

### Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | Critical, verified defect, high | *Stage 09 does not carry S13 through NMT broadcasts and non-reset NMT state changes.* Shared S13 covers every node-0 NMT command on a mixed interface, but 09 admits NMT by explicit intent only, with no broadcast case; its revocation tests cover reset, not Start, Stop or Enter Pre-operational. A Start can make a synchronous RPDO hazardous in Operational after an approval obtained in Pre-operational. Consume the S13 gate for node-0 NMT or exclude it; revoke before admitting an NMT command that can change the evaluated state and on observed external state changes; requalify from fresh evidence. Test targeted and broadcast Start with a Pre-operational node holding a synchronous RPDO, and no SYNC after revocation. | shared 02:233-235; 09:13-14, 67-68, 106-107 |
| F2 | High, verified defect, high | *The layout refusal also blocks explicit stops, contrary to the prompt 57 rule it cites.* 03 refuses every changing operation on CONTRADICTED and UNCONFIRMED, and 02b applies the identity cases to every changing operation from 04; prompt 57 exempts reviewed stops. MOONS' need not copy a Lichuan word, but must tell an ordinary refusal from a stop on an established running binding. Define a stop admission table per identity state (current, stale, unconfirmed, contradicted) for urgent and S3 stops; permit only stops whose device, binding and reaction premises hold; where no safe software stop exists, say so and name the application-owned fallback. Test invalidation during motion and a foreign server. | 03:30-31; 02b:33-34; 57:102-104 |
| F3 | High, probable defect, high | *Standalone enable does not distinguish enabling from starting motion in halt-start modes.* 04 makes 0x000F the normal final word and enable-with-halt an option, while velocity, torque and Q start by clearing halt; 06 uses 0x000F as the velocity start. With a target present, the default enable can start motion without a run request. Make 04's table explicit per mode and predecessor; require a non-starting form or refuse in those modes, not opt-in; cover already enabled and halted, unknown predecessor and present target; exact frames or no TX. | 04:13-16; 06:20; AGENTS.md:79 |
| F4 | High, probable defect, high | *Finite velocity can start without a remaining dispatch window for its mandatory halt.* 06 bounds dispatch and wire deadlines by the operation deadline but defines neither the duration's anchor nor a halt-dispatch cutoff, and does not check the duration and a stop budget before the start; a just-in-time start can leave no time for the halt that the shared rule then forbids. Anchor the duration, reserve stop dispatch and observation inside the deadline (or define the separate stop path), refuse the start edge without it. Test a barely timely setup, an uncertain start and a pressured halt. | 06:27-30; shared 02:108-109 |
| F5 | High, verified defect, high | *The bootstrap and layout-confirmation rules allow more than their evidence establishes.* 03's bootstrap covers whole config and state plans with product-specific objects; wrong-server tests cover only typed 0x2100. Vendor ID plus an EDS-default series string admits changing work, while the review says to use series, version fields and vendor ID together, and prompt 57's CONFIRMED needs uniqueness over the candidate references. Enumerate bootstrap objects with reviewed read effects and gate the other product reads; define the candidate set and unique confirmation, reconcile with the review; test an indistinguishable reference and a candidate without a reference with exact bootstrap traffic and no dependent TX. | 03:20, 21; review:193-194; 57:84-85 |
| F6 | High, probable defect, high | *The future watchdog arm path revokes its own required SYNC feed.* 11 requires an S13 approval naming the watchdog; 09 revokes approval before a 0x2060 write and needs fresh reads to resume. No stage defines the handoff to a verified armed state before the first timeout; Q9 has the same gap. Keep SYNC-fed arming and Q9 blocked until a reviewed transition defines pre-write approval, revocation, readback, reapproval and the first-feed deadline from a worst-case budget; test a lost arm reply and delayed reapproval. | 11:52-54; 09:73, 75; 14a:43 |
| F7 | High, verified defect, high | *First-contact release and homing depend on the facts they are meant to establish.* Q4 compares three release words through 04, which delivers one; Q10 uses 07's API, which needs the direction qualification Q10 produces, and 35/37 are refused where the sequence is unknown. A guard override is no evidence path. Split Q4 (production word versus gated comparisons) and Q10 (qualified methods, first direction discovery, 35/37), each with vendor evidence, a reviewed isolated experiment with restoration boundary, or NOT RUN. | 14a:20-21, 38; 04:30-31; 07:27-28, 63-65 |
| F8 | High, verified defect, high | *Stage 08 requires block-specific quiescence policies but does not define them.* Each write needs "the explicit quiescence policy of the block plus an idle drive", but no block gives the starting-state or readback predicates for motor data, current, brake, interlocks, direction or tuning; the alarm-mask setter is offered although its effect is unknown, against the contract's read-only rule. Add per-group criteria (drive state, fresh evidence, caller electrical limits, uncertain-write effect, inhibited dependents); keep unresolved protection settings read-only; test an energized idle drive and invalidation after a possible write. | 08:21-23, 48; contract:65; AGENTS.md:85 |
| F9 | Medium, probable defect, high | *Stage 12 leaves the discovery scan's S4 dependency ambiguous.* The scan is listed as direct work, while the inventory says `DiscoveryScan` is Lichuan-typed and owned by prompt 64. Name the consumed artifact, block the scan and its test until it lands, keep 02b's API probe. | 12:39-40; shared 01:142-143; shared README:37-39 |

### Unresolved questions (reviewer)

1. What positive, applicable evidence distinguishes AMP10 from other MOONS'-vendor servers that
   lack 0x2050? Stage 13 uses the absent-object abort as layout evidence, which is inferred and
   not shown unique over the candidate set.
2. Which 0x1010 sub-index writes, if any, will stage 02 admit despite the list versus sub-index
   access conflict? Until then stage 11 should separate codec and signature checks, a production
   admission test and restart-verified durability.

### Coverage gaps (reviewer)

The primary PDFs, rendered figures, EDS bytes, extracts and `SOURCES.json` were not supplied, so
page claims, encodings, access and hashes were not verified independently. The repository index
was truncated: owner and service code, adapter timing, packaging, verifier and workflows, seam
landings and progress rows were not checked. Only prompt 57 of the main sequence was supplied.
No MOONS' fixture, wiring, authorization, timing or restoration evidence existed; nothing was
built, tested or run on hardware.

## Round 3

A third audit of the set as revised by round 2, condensed the same way. Line numbers refer to the files at
the audited revision, before round 3 was applied.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-191534-e56216` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard (large input, about 197,135 tokens) |
| Date | 2026-11-19 19:15:34 (from the ID), 237.8 s, one attempt |
| Revision | `main` at `94138cd7b88852de54261a2309b693f32636095e` with uncommitted changes outside this set; the family files' SHA-256 equal their blobs at `94138cd7` |
| Cost | Estimated USD 0.43 (0.429276, configured pricing); 152,063 input and 12,515 output tokens (5,696 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-191534-e56216.json` (local) |
| Location checks | 47 verified, 0 relocated, unverified or invalid |

Promptset under audit: this directory's 20 files (README, execution contract, stages, rounds 1 and 2 audit and
application). Context: `canopen_shared/README.md`, `02_profile_integration_contract.md` and
`01_existing_code_and_tooling.md`, prompts 57 and 77, `ADD_DRIVE.md`, `COMMON.md`, the pack `README.md` and
`review.md`; project instructions `AGENTS.md`, `CLAUDE.md`. The shared contract and prompt 57 were read as they
stood before `c4e5574f`.

**Summary.** Earlier fixes hold: conflicted downloads are preflighted, the Lichuan value-1 binding is preserved
and moving homing no longer depends on the later settings context. The set is not ready for end-to-end
implementation: an urgent stop can clear a halt with unresolved effects, a finite run has no demonstrated stop
path after an SDO timeout, and identity, NMT recovery and qualification rules still diverge between stages.
**Verdict:** blocking issues. Evidence intake, the read-only probe and independently gated work can proceed;
resolve the stop contracts and their negative tests before admitting the affected motion or running physical
qualification.

### Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *Urgent quick-stop admission does not preserve the pending-halt invariant.* 04 says no stop releases a pending set-point, yet admits the fixed 0x000B from Operation enabled without excluding a known or possible set halt bit, while noting that 0x000B clears bit 8 and that whether a halted set-point resumes first is not stated. That blocks the S3 shape but not the ordinary urgent path; prompt 57's identity exemption does not remove the source-state check. 05 says quick stop ends disabled without 04's qualifications. Keep the identity exemption separate from command and predecessor admission; define the evidence for the quick-stop word when halt or a pending set-point is possible, leave unreviewed combinations unavailable and report the independent stop path; test TX or no TX for halted, unhalted and uncertain predecessors under each outcome; 05 consumes the stop result | 04:81-82, 95, 105-106; 57:105-106; 05:62 |
| F2 | High, probable defect, high | *A finite run can have a constructible halt request but no usable stop route.* 06 fixes the halt word after a lost start reply and tests it is composable, but with `SdoReuse` `NONE` the route stays unresolved after a timeout and 04 keeps the best-effort path denied; if ordinary urgent SDO work needs a resolved route, neither priority nor the stop budget makes the halt transmissible, and no other owner path is established. Specify and test the owner disposition from start TX through lost reply, route timeout, stop due and the urgent attempt; assert whether a stop frame leaves, keep `UNKNOWN` and `needsStop`, report when no software stop exists; keep the run gated without the stop path the timed-run contract needs | 06:83-85; contract:46; shared 02:235-236, 130-131 |
| F3 | High, verified defect, high | *Stage 04 leaves action admission tables and the explicit resume route unspecified.* The shared contract requires predecessor-to-command tables with already-reached and refused states; 04 has one for enable only. Release names one reviewed word without predecessor or already-released result; the two fault-clear methods give writes and some completion evidence but not where they are allowed or what happens with nothing to clear; the separate halt release has no admission or acceptance, though 05 refers to it and Q8 relies on it. Add MOONS' tables per action (state, mode, halt and fault evidence, first frame or no TX, settlement), refusal where the evidence is missing; deliver and test the resume route or mark it and Q8's resume subcase blocked | 04:45-46, 49, 86-87; 14a:43; shared 02:137-140; AGENTS.md:79 |
| F4 | High, verified defect, high | *Stage 13 and the execution contract do not consistently use prompt 57's identity decision.* 13 calls a returned 0x2050 value UNCONFIRMED, where a complete match to another declared candidate with no TSM/TXM-C match is CONTRADICTED; it forbids typed 0x2100 before the layout is "confirmed" right after admitting Part A reads on a colliding declared match; the contract requires a "confirmed layout" for vendor writes while pointing to admission by outcome, which admits some UNCONFIRMED results. Use the shared decision rule for every layout, supply references, keys and any separate evidence-backed gate by name; test a TSM/TXM-C declaration with a matching UM21 candidate, a nonmatching string and a colliding declared match, including whether typed 0x2100 is sent | 13:28, 31-33, 37; contract:85-86; 57:87, 111-114 |
| F5 | High, verified defect, high | *Targeted NMT Stop can leave no admitted CAN recovery path after identity invalidation.* 09 admits NMT Stop and says a Stopped node serves only node and error control. Across a host restart or rebind the identity observation needed for a targeted Start or Enter Pre-operational is lost and cannot be re-read while Stopped; broadcast is no general fallback. Define the recovery the application must have before sending Stop and how a Stopped, unconfirmed node is reported, without an implicit or broadcast bypass; test Stopped followed by rebinding or restart and an uncertain Stop with exact no-TX refusals | 09:13-14, 20-21, 85-86; 03:91-93; 57:117-118 |
| F6 | High, verified defect, high | *The S13 review is written for every MOONS' node but supplies only UM21 evidence.* 09 assigns the same effect inputs to each declared MOONS' node, including the UM21 watchdog 0x2060, while 13 makes AMP10 a read-only binding with another watchdog object and no S13 review; acceptance never proves that a monitored AMP10 node blocks SYNC or node-0 NMT when every UM21 node passes. Scope 09's adapter to UM21, keep S13's default deny for a declared AMP10 node, also when only monitored or not the target; test a mixed layout with zero SYNC and node-0 NMT TX and no UM21-only reads on the AMP10 route | 09:66-67, 72; 13:23-24, 74-75; shared 02:266-267 |
| F7 | High, verified defect, high | *Moving-homing admission both permits and forbids turning off the direction guard.* 07 says a missing approach qualification is prompt 77's settable direction guard once landed, then requires the same qualification unconditionally for switch methods, and its acceptance has refusal for an unknown direction but no guard-off case. Define one rule separating the optional direction check from the switch, wiring, index and stop and travel prerequisites; use the landed common guard; test unknown or stale direction with the guard on and off, naming the guard; keep Q10's first-evidence restriction | 07:30-32, 36-37, 83; 77:118-119 |
| F8 | High, probable defect, high | *Possibly executed coordinate changes do not explicitly retire the old origin.* 07 says when to publish a new reference and assigns invalidation only to the stationary methods and zero position; nothing says when the old reference becomes unusable after a possibly sent moving-homing trigger without observed completion. 10 invalidates "after every Q run" without saying whether a lost reply, cancellation or interruption counts. Retire dependent origins, references, prepared work and coordinate-based limits at the earliest point the command can have reached the drive, state what evidence gives a replacement; test retained references after a lost trigger, an unobserved home and a possibly executed Q start or zero-position write | 07:49-50, 57-59, 65-66; 10:39-40; AGENTS.md:77 |
| F9 | High, probable defect, high | *The settings current check does not cover commissioning writes that also change current.* 08 requires caller limits for its listed current writes, but motor selection and custom motor data, which also change current, need only commissioning intent; a whole-candidate check can pass them without establishing their effect on the limits, and quiescence does not address the current applied at the next enable. Make the indirect-current policy explicit; withhold the write or keep enable unavailable when it cannot be established; test unknown and out-of-limit results and an uncertain acknowledgement; keep it separate from the quiescence guard | 08:31-32, 61; review:457 |
| F10 | Medium, verified defect, high | *Release qualification still lists mode -2 without an executable path.* 10 and Q11 make SYNC Q refusal-only, yet 14 groups mode -2 with Q programs qualified by owner-supplied programs; a program supplies no activation contract or admitted route. The checklist also names operations that may still be gated. Mark mode -2 unsupported and NOT RUN, distinct from mode -1; tie torque, watchdog, store and restore to their stage's production gate or an approved evidence path, NOT RUN otherwise | 14:40-41, 45; 10:44-46; 14a:46 |

### Unresolved questions (reviewer)

1. After the 02b S2, S3 and S10 changes, what does the real owner do with an ordinary urgent halt on the same
   binding after a possibly transmitted start times out? The answer decides whether stage 06 can meet its
   timed-run acceptance without assuming that a queued request reaches the drive.
2. Does the reviewed-effect policy intentionally admit the alarm-mask writes on their documented LED effect
   while their protection effect stays unknown? Stage 08 and the lead admit the write with a warning, while
   the execution contract keeps objects without reviewed effects read-only; state the scope in one place.

### Coverage gaps (reviewer)

The original PDFs, rendered pages, EDS bytes, extracts and `SOURCES.json` were not supplied, so device claims
were checked for consistency only. The repository index was truncated: owner and service code, adapter timing,
packaging, workflows and seam landings were not verified at HEAD. The parent AGENTS, the conditional experiment
prerequisites and the main-sequence prompts other than 57 and 77 were not supplied. No MOONS' fixture,
authorization, trace, timing or restoration evidence existed; nothing was built, tested or run on hardware.

## Round 4

A fourth audit of the set as revised by round 3, condensed the same way. Line numbers refer to the files at
the audited revision, before round 4 was applied.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-193918-83d072` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard (large input, about 213,828 tokens) |
| Date | 2026-11-19 19:39:18 (from the ID), 200.2 s, one attempt |
| Revision | `main` at `a6b58cf12e7d71e24b7c95ddfce40bd5da173a0d` with uncommitted changes outside this set; the family files' SHA-256 equal their blobs at `a6b58cf1` |
| Cost | Estimated USD 0.44 (0.43605, configured pricing); 163,585 input and 10,888 output tokens (5,859 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-193918-83d072.json` (local) |
| Location checks | 28 verified, 0 relocated, unverified or invalid |

Promptset under audit: this directory's 20 files (README, execution contract, stages, rounds 1 to 3 audit and
application). Context: `canopen_shared/README.md`, `02_profile_integration_contract.md` and
`01_existing_code_and_tooling.md`, prompts 57 and 77, `ADD_DRIVE.md`, `COMMON.md`, the pack `README.md` and
`review.md`; project instructions `AGENTS.md`, `CLAUDE.md`. The shared contract and prompt 57 were read as they
stood before `328651b6`.

**Summary.** Evidence intake, object generation and the independently gated probe and read work can proceed;
the earlier fixes for conflicted writes, SDO timeout handling and refusal-only SYNC Q hold. Before dispatching
the affected motion, settings and network stages, resolve the gaps in halt-state admission, current-changing
writes and SYNC reapproval. **Verdict:** blocking issues: several changing paths lack an enforceable predecessor
or an explicit invalidation boundary; they block the affected operations, not the read-only work. A document
review; nothing was built, tested or run on hardware.

### Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, probable defect, high | *A successful enable does not establish the halt required by velocity, torque and Q setup.* 04 returns already enabled with no TX, so a clear or unknown halt record stays so; 06 and 10 prescribe enabling with halt set without saying what their operations do with that no-op result, and in an already enabled mode-3 binding no mode change happens, so the mode-select check does not help either. A run that treats the enable result as sufficient can update a live target before its start; 10 has the same gap before its mode -1 write and segment selection. Make a current, known halted predecessor an admission condition before any setup write that relies on halt; refuse or route to a separately reviewed halt otherwise; test all three halt records on an already enabled binding, a same-mode velocity request and the Q mode-select path | 04:24, 39-41; 06:19-20; 10:26-27 |
| F2 | High, verified defect, high | *The halt table neither establishes a valid source state nor excludes a new trigger edge.* 04 admits halt on a known controlword record, which describes prior host writes, not the drive's state; the review describes an interlock-driven state change independent of the controlword. After an uncertain position, homing or Q trigger the table selects 0x011F; if the trigger did not execute, bit 4 can still be low and the halt word makes the same rising edge as a start, and no precedence of halt over that edge is established. The timeout rule covers only a quarantined route. Keep the identity exemption, but define halt admission from evidenced source states and modes; cover both predecessors of an uncertain trigger; refuse the row where neither a non-triggering word nor documented precedence exists and name the alternative stop or the declared fallback; assert exact frames or no TX for autonomous state changes and both outcomes of a possibly sent trigger | 04:141; 07:49-50; review:225-226; 57:105 |
| F3 | High, probable defect, high | *Indirect-current writes can bypass the current limit on an already energized drive.* 08 lets the quiescence guard be switched off; a motor-selection or custom-motor write changes current, whose value is unknown until readback, and blocking the next enable or motion does not protect a drive already energized or running if the change applies at once. No evidence says these writes take effect only at a later enable. Keep the general guard settable; require a separate, non-bypassable de-energized condition for these two writes unless evidence establishes the resulting current first; keep the readback and unknown-state rules; test enabled-idle and running predecessors with the general guard off and partial and uncertain writes | 08:27-29, 32-36 |
| F4 | High, probable defect, high | *SYNC can be reapproved from observations taken before a queued NMT transition takes effect.* 09 revokes approval before admitting an NMT command but requalifies only on fresh reads and the NMT state the node reports, without requiring evidence from after the pending transition or blocking reapproval while it can still take effect. A Pre-operational observation taken while a targeted Start is queued or not yet reflected in the heartbeat describes the old state and would permit SYNC after the node goes Operational. Keep S13 approval revoked while an admitted transition can still change the evaluated state; require evidence beyond an explicit transition or retirement boundary; keep a possibly sent, unresolved transition ineligible; test reapproval with fresh old-state evidence while a targeted Start is queued and after dispatch before new-state evidence, and zero NMT TX for a broadcast Start with a valid RPDO | 09:88-89, 103-104, 108-109; shared 02:281-282 |
| F5 | Medium, verified defect, high | *Stage 04 delegates halt-release completion to a move contract that does not exist yet.* 04 owns and tests halt release before 05 supplies the move context and completion rules; its record keeps a word and the pending flag, not the target the completion check needs; 05's completion rule is anchored to a new trigger, while halt release deliberately makes no new edge. Make 04's halt release report the acknowledgement and any separately defined continuation observation, without move completion; assign resumed-target completion to 05 with a retained-target lifetime and a rule for continuation without a new trigger; test each in its stage | 04:4, 127; 05:51-52 |
| F6 | Medium, verified defect, high | *The targeted NMT bootstrap rule is not scoped to the reviewed layout.* 09 admits a targeted Enter Pre-operational on a fresh Stopped heartbeat with no current outcome for MOONS' bindings generally, while 03 admits only the probe for an AMP10 declaration until stage 13 and 13 Part A is read-only; 09 denies SYNC and node-0 NMT for AMP10 but not this targeted path. Scope 09's targeted NMT routes and the bootstrap exception to UM21; keep changing targeted NMT unavailable to AMP10 until a named stage admits it; test zero TX for that command on a Stopped AMP10 declaration with no current outcome | 09:22-24; 03:46-47; 13:15-16 |
| F7 | High, probable defect, high | *Zero position can rebase coordinates without stationary admission.* 07 requires stationary admission for methods 35 and 37 but gives the 0x200C write only an explicit intent and host reference retirement, which makes old host work unusable but does not make an executing onboard move stationary; the write changes position parameters whose full set is unknown, so a rebase during motion and a replacement reference from fresh reads could be accepted. Give zero position its own predecessor table: current stationary evidence and no active or uncertain coordinate-dependent onboard operation; keep retirement before possible TX and the stationary condition through replacement-reference verification; test running, halted-with-pending-target and unknown predecessors | 07:69-72, 82-84 |
| F8 | Medium, probable defect, high | *First contact lacks an explicit read-only path to establish its own entry evidence.* The contract requires firmware strings and node-ID evidence including 0x2020 before any later bench step, and 14a requires a recorded identity, yet Q1-Q3 are where transport, firmware strings and 0x2020 are first observed; a fixture with a label and switch settings but no CAN record has no entry path. Separate the pre-power and pre-CAN prerequisites from what the read-only first-contact sequence establishes; name the passive observations and bootstrap reads that may precede a recorded CAN identity and firmware; keep 0x2020 behind the layout admission and changing trials behind their gates | contract:139-141; 14a:6-7, 29, 37 |

### Unresolved question (reviewer)

1. For the future SSDC-C torque path, does stage 06's "confirmed UM21 layout" mean any admitting stage 03
   outcome or only CONFIRMED? Prompt 57 admits a current matching declaration under some UNCONFIRMED outcomes
   and the execution contract adopts that rule; the present 0x6087 gate masks the difference. Resolve the
   wording and test a colliding declared match before opening the torque path.

### Coverage gaps (reviewer)

The original PDFs, rendered pages, EDS bytes, extracts and manifest were not supplied, so device claims and
hashes were checked for consistency against the pack summary and review only. The repository index was
truncated: owner and adapter code, the adapter bitrate table, build and verification scripts and workflows were
not supplied, and the shared inventory is a historical map, not evidence of seam landings at HEAD. The parent
project instructions, the conditional experiment prerequisites and main-sequence prompts other than 57 and 77
were not supplied. No MOONS' fixture, wiring, authorization, measurement or trace existed; electrical behavior,
physical stop effects, service latency and restoration were not verified.
