# Independent audit of the Nanotec CANopen promptset, 19 November 2026

A condensed, faithful copy of the reviewer's report. It is untrusted reviewer input; the
verified dispositions are in [audit_application.md](audit_application.md). Line numbers
refer to the files at the audited revision, before the revision that applied the findings.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-175227-c23b91` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-astra` requested and used, reasoning effort xhigh, depth complex ("focus mentions safety-critical control") |
| Date | 2026-11-19 17:52:27 (from the ID), 353.2 s, one attempt |
| Revision | `main` at `eb3dae18c7933ae0dbb887c1415246d06812d45f`, no uncommitted changes recorded; the audited family and shared files are byte-identical at `139748e9` |
| Cost | Estimated USD 1.56 (1.555856, configured pricing); 113,602 input and 16,176 output tokens (7,268 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-175227-c23b91.json` (local) |
| Location checks | 64 verified, 0 relocated, unverified or invalid |

Promptset under audit: the 19 files of this set (`README.md` 147 lines, sha256 `1079443f...`;
`execution_contract.md` 200, `fb30fb32...`; stages 01 to 13). Requirements: canopen_shared
`README.md` (79, `038b9207...`) and `02_profile_integration_contract.md` (209, `3f0a666c...`),
`ADD_DRIVE.md`, `COMMON.md`. Context: the pack `README.md` and `review.md` (380,
`7deb2d9b...`), `docs/audit/2026-11-19_family_selection.md`, canopen_shared
`01_existing_code_and_tooling.md`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The set separates documented device behavior, host-tested software and
qualified hardware well, and gives good defaults for unknown firmware, lost replies, LSS
and the conflicting bitrate codes. It is not ready for independent execution: registration
precedes a required owner safety guard, the first enable depends on later stages, and
several first-contact experiments need operations the production API must refuse.
**Verdict:** blocking issues. Stage 01 and much of the ledger work can proceed; the
blockers affect the first registered binding and the first actuator writes, which need
explicit, tested ordering of their guards and a separate disposition for experiments that
cannot run through an admitted production operation.

## Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *Registration exposes a Nanotec binding before the owner defaults its best-effort stop to none.* The owner accepts Lichuan's stop bytes on every channel; 02b registers a real channel but lands only S1, S2, S6 and S10, deferring all of S3 to 04. The absence of a Nanotec stop preparation is no guard on the owner entry point, and the later candidate has the same bytes. Claim or consume S3's default deny before the first binding can be admitted in 02b; test a direct best-effort stop on that binding, also after timeout and rebind (`INVALID`, zero TX), Lichuan unchanged; leave the shape to 04. | 02b:8; shared README:53; 04:67-69; shared 02:172-174 |
| F2 | High, verified defect, high | *Stage 04 cannot pass its enable acceptance with only its predecessors.* The first enable word needs 04b's exclusive-control decision and 09b's supervision state; 03 only observes; 04b runs after 04 and uses enable for auto setup. No tested intermediate artifact or return step lets 04 finish its positive enable tests. Split out a read-only supervision report and the pure exclusive-control predicate right after 03 and test both before 04 exposes enable, or split 04 and schedule the enable before auto setup. The heartbeat producer and setters need not move. | 04:21-23; 03:58; 04b:4, 48-50; 09b:35-36 |
| F3 | High, verified defect, high | *The enable sequence specifies destination checks but no admissible starting state.* A non-faulted drive already in Operation enabled would receive shutdown first; checking the statusword after that write does not prevent the transition. Quick stop active has its own edge-dependent exit. Give 04 a source-state and intent table before the first TX: which state admits the three-step sequence, what an already-enabled request returns without cycling power or brake, which states refuse or need the quick-stop exit; specify whole HALT words and allowed modes instead of modifying the current word; test the first frame, or none, per starting state and mode. | 04:19-22, 36-37, 46-48; AGENTS:79 |
| F4 | High, verified defect, high | *The exclusive-control predicate does not decide several observed command paths.* 03 exposes controlword-, mode- and NanoJ-mapped RPDOs and out-of-range modes, but 04b tests only 2291h with Plug & Drive on, not a controlword RPDO in Operational or an active external-signal mode; refusing a mode select is no evidence that PD4-C 4015h is off. There is no rule retiring a cached RUN = 0 and queued work when the library starts NanoJ, before RUN is seen or after a lost reply. Specify an admission table per path (active/unknown, evidence of exclusion, invalidators) with PD4-C special-mode evidence, effective RPDO maps, NMT state and pending synchronous data; invalidate ordinary admissions before handing authority to NanoJ or another path and require fresh evidence to return; define narrow identity-gated NanoJ-stop and Plug & Drive-off operations; test queued work, delayed RUN and lost replies. | 03:56-58; 04b:16-18, 35-37, 58-60; 09:28-30 |
| F5 | High, verified defect, high | *S13 lacks an acceptance case for a known but harmful SYNC effect.* 09 refuses unknown mappings or types, but a fully known synchronous RPDO can apply a staged controlword or setpoint at the next SYNC. Define the S13 consumer gate on the existing producer; test known harmless, unknown or stale, and known harmful across all bound nodes, including a pending synchronous command on a node other than the target; zero SYNC TX on refusal; invalidation on remap, rebind or reset; record the interface and checks in the seam table. | shared README:63; 09:43-46, 26-27, 76-78 |
| F6 | High, verified defect, high | *Landing S11 is not the runtime prerequisite for configuring a heartbeat consumer.* The 1016h setter gate is that S11 exists and passed, which holds with no active producer on this interface, a different producer node, or after bus-off; comparing the consumer time with a nominal period does not show the schedule meets it. Require current owner evidence for interface, producer node, generation, active schedule and a qualified service margin before a nonzero 1016h; specify producer identifier reservation, missed-service and TX-retirement behavior and bus-off or reconfiguration invalidation; test absent, wrong-node, stale and interrupted producers with S11 compiled in; keep configured, transmitting and demonstrated reaction separate. | 09b:40-41, 44-49, 76-79 |
| F7 | High, verified defect, high | *The save sequence states the standstill requirement without saying how to establish and hold it.* The concrete check is refusal in Operation enabled, yet 04 says a release is not standstill, so a coasting axis outside Operation enabled is a missing negative case. Presenting the application snapshot opens an interval in which queued work changes what is persisted, and a lost save reply leaves an interval in which motion may overlap the save. Require fresh, fit-for-purpose standstill evidence and exclusive ownership; bind approval to snapshot, category, binding and configuration generation; exclude starts, settings and writers from snapshot to confirmed completion; let unresolved saves block conflicting work; test coasting, changed snapshot, queued start and lost reply; apply the gate to the store inside a communication change. | 10:46-49, 54-57; 04:37-39; review:250 |
| F8 | Medium, verified defect, high | *The binary reading of 1020h lacks a known saved stamp.* The master writes date and time before the save; a zero read without a baseline cannot tell an unstamped configuration from one invalidated by a later write. Report the raw stamp and UNKNOWN unless a recorded valid baseline from a confirmed save on the same unit exists; make any stamp write explicit with its comparison and invalidators; test no baseline, zero, failed save, later savable write and boot-time override; never verify individual values with it. | 10:65-67; review:253 |
| F9 | High, verified defect, high | *First-contact experiments depend on the production operations they are meant to unblock.* 12A permits only production APIs, but Q5 writes 87h, which 10 refuses; Q11 needs the gated PDO save; Q12 needs an LSS exchange the library lacks. Q5 needs stage 10 and Q7 a stage-05 PP move, yet both are scheduled after 04/04b; Q5's fixed restore to 88h need not restore the original value. List per-question prerequisites with the exact route; move the PP stop trial after 05; mark Q5, the disputed part of Q11 and Q12 NOT RUN through production APIs and resolve them by vendor evidence or a separately assigned, isolated, reviewed experiment, never by weakening a refusal or a raw console escape; require an original-value snapshot and verified recovery plan; report an observed bitrate as specimen behavior. | 12a:7-8, 14-15, 26, 32-33; 10:25-27, 39-41, 58-60 |
| F10 | High, verified defect, high | *Model admission is inconsistent about missing EDS files and incomplete about hardware revision.* 02 defines a scope by its stored EDS yet invites admitting -5VDI from a manual statement; 13 lists the stored EDS as a prerequisite. The electronic tuple omits checking the declared nameplate revision against the manual's minimum, so an out-of-scope hardware revision can match. Use one admission table (model and suffix, nameplate revision range, stored EDS scope, firmware build, product codes); keep -5VDI refused until its scope exists; test missing, below-minimum and contradictory hardware declarations with zero writes; enumerate the diagnostic reads allowed per refused or unknown row. | 02:13-15, 23-25; 13:15; contract:88-90, 94-95; review:34 |
| F11 | High, probable defect, high | *Configured closed loop can be mistaken for active closed loop at torque admission.* 06b's conditions pass for a declared closed-loop axis with auto-setup evidence while it still runs open loop awaiting an encoder index; the prompts distinguish configuration bits from the bit-15 observation valid only in Operation enabled. Require fresh, same-session active-loop evidence besides configuration; refuse or defer PT and block homing without it; test an enabled configured axis before and after confirmation and stale evidence after restart or motor-settings change. | 06:41-43; 04:26-27; 03:44-45 |
| F12 | High, verified defect, high | *The optional cyclic implementation has no owner path for transmitting RPDO setpoints.* 06c needs outgoing RPDO setpoints; 09 delivers map readback, remapping, observation and a SYNC gate, and the owner admits only SDO, NMT and SYNC. Keep the no-API disposition unless the cyclic assignment also owns or consumes a tested RPDO transmit path through the single owner (route, map and identity binding, SYNC coordination, lease expiry, stale setpoint retirement, urgent stop, bus-off); no family-local raw transmitter or second-protocol seam. | 06:50-54, 57-60; 09:67-68; shared README:59 |
| F13 | High, probable defect, high | *Package manifests are scheduled one stage after the first shippable headers.* 02 adds headers under the recursively exported include tree and requires exact-commit CI, but the install rule and exact package lists come only in 02b, so the archive gains unexpected members and the CMake install omits them. Move install rule, `HEADERS` and archive entries, standalone-header and package-consumer checks into the generated headers' commit; extend them in 02b; split tooling commits only where each registered checker has its inputs and the package gates pass. | 02:81-84; 02b:78-79; shared 01:62 |
| F14 | Medium, verified defect, high | *Stage 11 treats family console routes as an alternative before S4.* The contract blocks family console entries and parity until S4, but 11 says the family routes remain the motion path until S4 lands. State that before S4 only the native API and owner-backed tests exist and console routes, common dispatch and parity stay Blocked; list the return work; no second session or temporary family `ProbeApp`. | 11:17-20; contract:115-118; shared README:43-46 |
| F15 | Medium, verified defect, high | *The bench disposition conflicts with the no-port and owner-deferral rules.* 11 conditionally tells its executor to run the CL86-C regression although family work never sends frames to those nodes or opens COM15; the contract postpones deferred-table entries until a Nanotec fixture is recorded, while AGENTS sends owner steps to the table during the absence. Remove the bench imperative (an independently authorized Lichuan run may be cited as Lichuan-only evidence); record owner-dependent fixture setup and its cases as deferred/NOT RUN now, with no port access or waiver extension. | 11:73-75; shared 02:152-153; contract:166-168; AGENTS:81 |

## Unresolved questions

1. Which committed revision supplies the required CANopen principles input to a clean
   checkout? The README (85-87) calls it working-tree-only while the contract (18-20)
   requires it; pin it or fall back to the Nanotec pages for the dependent items.
2. Is there model-applicable evidence that IP uses the same bit-12 "target followed"
   meaning as CSP/CSV/CST? 06 (50-55) groups them; the review describes IP separately
   (167, 168-171). A shared completion rule should stay unimplemented for IP unless its
   own page confirms it.
3. What observable boundary establishes firmware readiness when the boot-loader boot-up
   is suppressed or one boot-up is missed? 09 (58-62, 78-79) counts two, which cannot
   hold in every documented configuration; reacquisition needs positive session and
   identity evidence without an invented delay.
4. Which per-model evidence closes NanoJ autostart, the PDO save category and heartbeat
   arming and reaction order? Fakes establish refusal, uncertainty and invalidation, not
   what a restarted drive or one that loses its host physically does.

## Coverage gaps (reviewer's)

- The rendered manuals, individual EDS/XML files, package index and SOURCES.json contents
  were not supplied; device claims were compared with the review only.
- The file index was truncated; owner, CANopenNode, TWAI, build and workflow contents and
  the current Git log were not supplied, so architecture and packaging conclusions use
  the shared inventory.
- The parent Projects instructions, the shared audit application, several main-sequence
  prompts, progress reports, sibling revisions, scripts and workflows were not supplied.
- No Nanotec fixture record, service budget or traffic evidence existed; no build,
  checker, test, CI job or hardware procedure was run.

## Round 2: audit of the revised promptset, 19 November 2026

A condensed, faithful copy of the second report; untrusted reviewer input like round 1.
The verified dispositions are in [audit_application.md](audit_application.md), "Round 2".
Line numbers refer to the files at the audited revision, before round 2 was applied.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-184316-126cde` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard ("large input (~175034 tokens)") |
| Date | 2026-11-19 18:43:16 (from the ID), 224.1 s, one attempt |
| Revision | `main` at `edcf9621a1f5f598ca77d5e9261f885b7946f601`, no uncommitted changes recorded; this set's files are byte-identical at `a7656efb` |
| Cost | Estimated USD 0.38 (0.380494, configured pricing); 137,277 input and 10,594 output tokens (4,100 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-184316-126cde.json` (local) |
| Location checks | 38 verified, 0 relocated, unverified or invalid |

Promptset under audit: the 21 files of this set after round 1 (`README.md` 150 lines,
sha256 `ee9492f7...`; `execution_contract.md` 224, `53c41303...`; `audit.md` and
`audit_application.md`; stages 01 to 13). Context: canopen_shared `README.md` (81,
`d7ca1b63...`), `02_profile_integration_contract.md` (289, `9afe3979...`) and
`01_existing_code_and_tooling.md`, `ADD_DRIVE.md`, `COMMON.md`, the pack `README.md`
and `review.md` (380, `7deb2d9b...`), prompt 57 (194, `6190b377...`); project
instructions `AGENTS.md`, `CLAUDE.md`. The cross-cutting revision `a7656efb` of the
shared contract and prompt 57 came after this audit.

**Summary.** Round 1's sequencing problems are fixed: S3 defaults to deny before the
first binding, the exclusive-control predicate and supervision snapshot exist before
enable, and the first generated headers carry their package entries. The set is still
not ready to execute unchanged: some mode and stop paths bypass or conflict with their
own admission rules, the firmware gates do not consistently reach the operations that
depend on them, and a category save can persist PDO settings despite the stated gate.
Independent ledger, packaging and read-only work can proceed.
**Verdict:** blocking issues, localized to changing operations and their acceptance
gates; resolve the contradictory stop rules, the auto-setup exceptions and the
whole-category save, and test the actual owner admission paths before exposing them.

### Round 2 findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The S13 work order covers SYNC but does not make mixed-bus NMT broadcast admission executable.* Shared S13 applies to every node-0 NMT command on a mixed interface; 09 specifies and tests only SYNC effects, and 11's written broadcast analysis does not gate the owner's NMT path or prove zero TX on refusal. Consume S13 at the owner's mixed-interface NMT entry point, supply Nanotec's per-command effects and evidence or refuse broadcasts until they exist; real-owner tests for unknown or stale evidence and an unauthorized effect on a non-target node, zero NMT TX on refusal; keep targeted NMT separate. | shared 02:233-234; 09:46-47; 11:33-35 |
| F2 | High, verified defect, high | *Regular mode select and enable do not enforce the later mode-specific gates.* 04 selects a mode in Operation enabled on 6502h and capability checks alone (PT on configured closed loop) while 06b needs fresh active closed loop before the change to PT; enable needs only a known mode and does not separate advertised cyclic modes from those withheld pending 06c. A supported-mode bit is no host admission. Consume an explicit host admission table for mode select and enable in a selected mode, default undelivered modes to unavailable, enforce 06b's rule at the public mode transition, keep cyclic activation unavailable until 06c's gates pass; test the generic routes. | 04:26-27, 72-74; 06:45-47, 74-76 |
| F3 | High, probable defect, high | *Auto setup has no specified transition path through its own mode and authority guards.* It selects -2 and enables, yet mode select refuses -2, the predicate refuses once -2 is active, and auto setup advances the control-path generation before its first TX. Reusing the public actions is refused; bypassing them has no operation-local admission, source-state table or handling of a partial mode change; a possible persistent change after a lost trigger reply is not distinguished from a confirmed save. Specify an auto-setup-only sequence or keep it blocked: admissible initial state, authorization scope after the generation change, stop, timeout and fault behavior per step, coordinate invalidation no later than the trigger, ordinary control inhibited across uncertainty; test a lost mode-change and a lost trigger reply. | 04b:35, 38-40, 74-78; 04:71-73 |
| F4 | High, verified defect, high | *FIR-v1650's missing Plug & Drive object turns a feature gate into a broad control lockout.* 13 omits absent objects, yet its Plug & Drive row makes 04b's predicate depend on a 2290h read that aborts, so skipping it leaves the predicate without an admitting value and ordinary enable, motion and settings are refused on old firmware. Bit 15 is read as "active" in 03 although 13 records "available" on FIR-v1650. Use a firmware-scoped applicability state (evidence-backed absence versus unread, aborted or unsupported), decide absence explicitly in the predicate, block only the dependent operation otherwise, dispatch bit 15 the same way; test an ungated FIR-v1650 operation without a 2290h upload, refusal on a merely unread observation, the legacy bit 15. | 13:24-27, 46, 48; 04b:11-14; 03:55-56 |
| F5 | High, verified defect, high | *The Nanotec identity rule does not reconcile its blanket refusal with prompt 57's explicit-stop exception.* The contract and 03 refuse dependent work and keep only identity reads, while prompt 57 keeps reviewed stops and state reads after contradiction; 04's "stops need no exclusive control" settles neither identity admission nor the fresh state its stop table needs. State which parts of prompt 57 are common, reconcile the exemption, enumerate the state reads and stop paths that stay admitted and their evidence, name any refusal and the need for an independent stop path; test both stop entry points under contradiction with no unrelated write. | contract:97-98; 03:87-88; prompt 57:101-104; 04:56-58 |
| F6 | High, verified defect, high | *The stop rules leave both halted and unresolved source states underspecified.* No stop clears HALT bit 8, yet the table sends 000Bh (bit 8 clear) from Operation enabled, where HALT may be set, and the pages do not say how quick stop and halt removal are prioritized. The unresolved S3 path checks only shape, binding and channel generation, which establishes no predecessor. Add rows for a halted predecessor and an unresolved route; say whether quick stop may supersede HALT with device evidence or refuse; for S3 define sufficient current state evidence or keep the shape disabled; test halted, wrong-state and unknown-state paths. | 04:52-56, 86-90; shared 02:114-116 |
| F7 | High, verified defect, high | *A whole-category save can bypass the unresolved PDO persistence gate.* 10 gates "the PDO save" because the sources disagree on the map's category, but exposes the implicated category saves and save-all; Q11 changes a non-PDO value and saves 1010h:02, which saves the whole category. A map changed before the session or left uncertain by a partial remap is not excluded. Gate the category write: require evidence that it cannot persist a PDO change or block it and Q11's route; zero-write tests for a changed map and an unknown persisted baseline, save-all included. | 10:45-47, 81-84; 12a:42; review:248 |
| F8 | High, probable defect, high | *Finite velocity and torque do not reserve a dispatch interval for the stop.* 06 yields the stop at duration expiry while the shared contract forbids changing steps at the deadline; the duration anchor, its relation to the deadline and the yielded stop's admission are undefined, so a request can start with no legal opportunity to stop. Define the anchor, separate run expiry from the deadline, validate a stop budget and the stop's admissibility before the first changing step, or specify an independently admitted urgent stop; test delayed start, queue saturation, service resuming at the deadline and a lost start reply. | 06:21-24, 41-43; shared 02:107-109 |
| F9 | Medium, verified defect, high | *First-contact Q2 and Q13 name read routes that do not deliver all their observations.* Q2 needs 2007h:01 and 2084h, absent from 03's plan, and 09 adds no typed uploads; Q13 names only 03 but needs the FIR-v2538-only 2007h:02 and the PDO COB-ID readback that 09 owns. Assign typed read plans in the owning stages and make Q2 and Q13 wait for them, or mark the observations NOT RUN; test omission where an entry is absent. | 12a:33, 44; 03:41; 09:82-83 |
| F10 | Medium, verified defect, high | *Registration invokes identity-refusal acceptance before delivering the identity verdict.* 02b applies the shared registration acceptance but delivers a probe without identity conclusion; its cross-family tests check the binding, not a correct label with a wrong, missing, ambiguous or stale device. Deliver the minimum verdict with real-owner refusal tests, or split acceptance explicitly: 02b proves probe and binding with dependent writes unavailable, 03 closes the identity gate before any write is callable. | 02b:4-7, 48-50; shared 02:72-74, 284-285 |

### Round 2 unresolved questions

1. Can the Nanotec boot loader answer the identity read, and what positive observation
   distinguishes firmware readiness when the later firmware boot-up is missed? Stage 09
   no longer counts boot-ups, but an identity reply after the last observed boot-up
   proves readiness only if it cannot come from the boot loader.
2. What evidence bounds heartbeat delivery at the drive rather than owner service, and
   does the producer check stay valid until the queued 1016h write is dispatched? The
   period-plus-service-gap check suffices only if the measured bound covers TX and
   delivery delay under load, and producer evidence can expire while the write waits.

### Round 2 coverage gaps (reviewer's)

- The rendered manuals, EDS and OD XML files, manifest and package index were not
  supplied; device constants and cross-model applicability were not verified against
  primary sources.
- The repository index was truncated; owner, CANopenNode, TWAI, application, checker,
  packaging, verifier and workflow sources and the Git state were not supplied.
- The parent Projects instructions, the shared audit application, current progress and
  prompts 51, 56, 64, 73 and 77 were not supplied.
- No Nanotec fixture, raw capture, measured service budget or electrical timing
  evidence existed; no build, checker, test, CI job or hardware procedure was run.

## Round 3: audit of the round-2 promptset, 19 November 2026

A condensed, faithful copy of the third report; untrusted reviewer input like rounds 1
and 2. The verified dispositions are in [audit_application.md](audit_application.md),
"Round 3". Line numbers refer to the files at the audited revision, before round 3 was
applied.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-192232-a1c214` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used (baseline), reasoning effort high, depth standard ("large input (~204009 tokens)") |
| Date | 2026-11-19 19:22:32 (from the ID), 179.0 s, one attempt |
| Revision | `main` at `6ff80ed1dd19026da4e970220fb947437abf87ba`, uncommitted changes recorded in other sets; this set's files and the shared, prompt 57, prompt 77 and pack context are byte-identical at `6ff80ed1` |
| Cost | Estimated USD 0.42 (0.415602, configured pricing); 157,926 input and 9,975 output tokens (3,779 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-192232-a1c214.json` (local) |
| Location checks | 33 verified, 0 relocated, unverified or invalid |

Promptset under audit: the 21 files of this set after round 2 (`README.md` 150 lines,
sha256 `07bf6c1c...`; `execution_contract.md` 235, `ffbe5f5c...`; `audit.md` and
`audit_application.md`; stages 01 to 13). Context: canopen_shared `README.md` (81,
`b0d83a01...`), `02_profile_integration_contract.md` (341, `6e125d23...`) and
`01_existing_code_and_tooling.md`, `ADD_DRIVE.md`, `COMMON.md`, the pack `README.md` and
`review.md` (380, `7deb2d9b...`), prompt 57 (214, `27398264...`), prompt 77 (207,
`bbb697d7...`); project instructions `AGENTS.md`, `CLAUDE.md`. The shared round 4
revision `c4e5574f` is inside the audited revision.

**Summary.** The S3 and S13 default denials land with registration, the
exclusive-control predicate and supervision snapshot precede enable, and package
entries accompany the first headers. The remaining issues affect changing operations:
the family drops prompt 57's NMT bootstrap exception, treats two binding S11 checks as
optional future work, and does not fully specify stop admission, active-mode entry or
what proves a PDO configuration safe to persist. Independent evidence, tooling and
read-only work can proceed. **Verdict:** blocking issues: some acceptance criteria
permit behavior the binding shared contract refuses, and some state and persistence
rules can report a condition the evidence does not establish; these block the affected
routes only, not grounds for a firmware lock or for delaying independent work.

### Round 3 findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The family omits prompt 57's NMT bootstrap exception.* The contract restates admission by outcome as refusing every changing request without a current outcome, except reads and stops; prompt 57 also admits a targeted Enter Pre-operational on fresh Stopped evidence unless CONTRADICTED, and 09 leaves targeted NMT to its own undefined admission, so identity can be required before the command that makes the identity read possible. Reference prompt 57's table instead of an incomplete restatement; assign the case to the owner's targeted NMT route; test admission with fresh Stopped evidence and no outcome, refusal under CONTRADICTED or stale or missing state evidence, no implicit recovery send; keep the note that Stopped-state heartbeat or guarding is not guaranteed. | contract:98-100; prompt 57:119-121; 09:82-84 |
| F2 | High, verified defect, high | *09b treats two binding S11 checks as an open request.* The shared row requires the period check to count TX delay under load and the producer to be rechecked when the 1016h write is handed to the controller; 09b calls both open and tests only producer status and period plus service gap at admission, which passes if the producer stops while the write is queued or delivery exceeds the consumer time. Make both prerequisites of a nonzero 1016h; keep the setter blocked until they pass; zero-write tests for producer invalidation between admission and handoff and for a delivery budget failing despite an acceptable service gap; keep configured supervision separate from demonstrated reaction. | shared README:62; 09b:61-64, 97-98 |
| F3 | High, probable defect, high | *Timed-run acceptance does not prove a stop can leave after a lost start.* 06a and 06b cite the shared rule but admit and test the ordinary HALT and QUICK_STOP rows, which need fresh SDO evidence; with S10 `NONE` a lost start reply can leave the route unresolved, and S3 is opt-in per binding, so stage 04 having landed does not supply it. With S10 `NONE`, refuse before the first changing frame without the binding's reviewed S3 shape or a declared independent stop; with S3 opted in, lose the start reply and assert that the run retires and at most the reviewed best-effort frame is sent, uncertainty kept, no standstill claimed. | shared 02:131-134; 06:29-33, 99-104; 04:149-151 |
| F4 | High, probable defect, high | *Selecting a mode while enabled does not account for a retained setpoint.* 04 checks for a fresh zero velocity target when enabling in VL or PV but not when selecting either mode while already enabled; the bit-4 check is not a target check. PT requires active closed loop but no order neutralizes or validates an existing torque target before selecting PT; stage 10 says these targets are savable, and the evidence does not say what an enabled mode change does with a retained nonzero target. Define admission and write order per destination mode entered while enabled, with the target and halt disposition; keep the paths gated where evidence cannot establish safety; test nonzero retained targets, lost replies during entry and zero TX on refusal. | 04:88-92, 105; 06:54-56; 10:89-90 |
| F5 | High, verified defect, high | *A set halt bit is reported as an attained halt even in modes that ignore it.* The bit-8 Operation enabled row returns "Already halted" without testing the mode, although HALT is limited to modes 1-4 and bit 8 has no function in homing, cyclic modes or auto setup; the Quick stop active release uses 0000h without distinguishing bit 8, though no release clears a set halt bit. Distinguish a set bit from an attained halt; report an already requested halt only in a reviewed mode without claiming standstill, otherwise refuse; split the Quick stop active release by halt evidence with an evidenced halt-preserving word or refusal; assert results as well as frames. | 04:160, 128-130, 57-59, 161 |
| F6 | High, probable defect, high | *The first motion stages can land before the limit admission they are meant to use.* 07 adopts prompt 77's limit admission but covers homing prerequisites and reaction reporting, not admission of the PP, PV or VL routes 05 and 06a already expose; prompt 77 requires the installed library to refuse active, stale or undecodable limits with a settable guard, and reporting 3701h (factory: no reaction) does not. Assign the Nanotec input and limit admission before the first dependent motion route or gate those routes; reuse prompt 77's policy and names with Nanotec input facts; test active, stale and undecodable inputs, guard-off reporting and admitted stops; import no Lichuan registers. | 07:4-6, 48-52; prompt 77:113-115; 05:36-37 |
| F7 | High, verified defect, high | *The enabled PDO-persistence guard does not prove the baseline it claims.* 10 says the guard admits a save only when the PDO configuration is the one loaded at the last observed boot-up, but its checks show only an observed boot-up and no write by this library since; with the single-master guard off another writer can change a map and all checks still pass, so Q11 can save an unintended map with the PDO guard on. Keep both guards independently settable; with the PDO guard on, require evidence covering the claimed baseline through the save handoff or refuse; if it only protects against this library's changes, say so and do not use it as Q11's proof; test the two settings independently, including an out-of-band map change with only the single-master guard off. | 10:71-75; 04b:21-24; 12a:42 |
| F8 | High, probable defect, high | *A run-time PDO COB-ID change has no identifier-reservation and activation handoff.* 09 exposes configurable COB-IDs that take effect only after restart or Reset Communication; 11 checks identifiers at binding and 09 revokes evidence on map changes, but neither defines how a bound interface checks and reserves a candidate or keeps the old route distinct while activation is pending, so a read-back value that is not yet active cannot qualify routing or renewed S13 approval. Consume the owner's identifier plan before the first remap write; distinguish configured, pending and active IDs, keep reservations and routes and S13 approval unqualified through partial writes or unknown activation; test an in-use candidate, readback before activation and reacquisition after reset without qualifying a delayed sample. | 09:18-20, 31-33; 11:44-46; AGENTS:79 |
| F9 | High, probable defect, high | *A post-boot identity reply is insufficient to establish firmware readiness.* 09 replaces boot-up counting with a fresh identity reply after the last observed boot-up, but the files do not say whether the boot loader can answer it; if it can and the firmware boot-up is missed, the sequence passes without a firmware-ready boundary. Separate a responding server, a product identity outcome and a firmware session ready for changing work; leave readiness unknown while the boundary can be the boot loader's; test a possible loader reply with no later boot-up, keeping diagnostics and prompt 57's stop rules available; no guessed delay or build whitelist. | 09:96-99; audit_application:131-136; AGENTS:71 |

### Round 3 unresolved questions

1. What model- and firmware-specific observation distinguishes the boot loader from the
   application firmware when the final boot-up is not seen? It decides which post-reset
   observations requalify a session for changing work; a synthetic server tests only the
   unknown case.
2. What do the device sources establish about retained destination targets during an
   enabled VL, PV or PT mode change, and about the active versus read-back COB-ID before
   reset? These facts decide whether the transition and remap paths can be admitted,
   need narrower entry conditions or stay gated.

### Round 3 coverage gaps (reviewer's)

- The rendered manuals, EDS and OD XML contents, source manifest and package index were
  not supplied; device observations were checked against the stage text and review only.
- The repository index was truncated; owner, CANopenNode, TWAI, application, package,
  verification and workflow sources, live PROGRESS rows and Git history were not
  supplied.
- The parent Projects instructions, the shared audit application and prompts 51, 56, 64
  and 73 were not supplied.
- No fixture record, wire capture or measured service or heartbeat-delivery budget
  existed; no build, test, CI job or hardware procedure was run.
