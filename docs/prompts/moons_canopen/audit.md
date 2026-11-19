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
