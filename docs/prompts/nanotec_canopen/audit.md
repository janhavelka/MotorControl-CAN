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
