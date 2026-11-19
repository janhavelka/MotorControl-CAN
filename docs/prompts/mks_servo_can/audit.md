# Independent audit of the MKS SERVO42D/57D CAN promptset, 19 November 2026

A condensed, faithful copy of the reviewer's report. It is untrusted reviewer input; the verified dispositions are in
[audit_application.md](audit_application.md). Line numbers refer to the files at the audited revision, before the
revision that applied the findings.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-175945-8183dc` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-astra` requested and used, reasoning effort xhigh, depth complex ("focus mentions safety-critical control") |
| Date | 2026-11-19 17:59:45 (from the ID), 348.0 s, one attempt |
| Revision | `main` at `139748e9c2ebddbd8e33e1e856cfd9126b6bd078`, no uncommitted changes recorded |
| Cost | Estimated USD 1.59 (1.586256, configured pricing); 111,492 input and 17,358 output tokens (7,913 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-175945-8183dc.json` (local) |
| Location checks | 69 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_evidence_and_command_ledger.md` (120, `726a8c5f`),
`02_codecs_and_dialects.md` (74, `bad3422a`), `02b_protocol_domain_and_owner.md` (91, `0bfe194e`),
`03_reads_identity_and_dialect.md` (70, `4483a12e`), `04_enable_stop_and_uncertainty.md` (61, `9aefc145`),
`05_finite_position.md` (72, `2062007e`), `06_speed_mode.md` (63, `6838a46d`), `07_homing_and_limits.md` (68,
`d9d0b1fc`), `08_settings_and_persistence.md` (64, `bceb69c4`), `09_async_reports_and_reply_matching.md` (72,
`5047f2d4`), `10_bus_settings_and_restart.md` (64, `9df991ab`), `11_console_and_integration.md` (63, `5f059826`),
`12_qualification_and_release.md` (63, `67d33c83`), `12a_first_contact.md` (60, `9383249b`), `README.md` (154,
`9b4e5985`), `execution_contract.md` (146, `d8b6a583`). Requirements: `canopen_shared/README.md`,
`canopen_shared/02_profile_integration_contract.md`, `ADD_DRIVE.md`, `COMMON.md`. Context:
`docs/reference/mks_servo_can/README.md` and `review.md`, `docs/audit/OTHER_CAN_PROTOCOLS.md`,
`63_mixed_protocols.md` (68 lines, before `6786fde1`), `canopen_shared/01_existing_code_and_tooling.md`; project
instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The promptset makes the right architectural choices: MKS has its own codec and service, CANopen and MKS
stay on separate physical interfaces, and a local transmit result is not a device acknowledgement. It is not yet safe
to execute end to end. The main blockers are an owner capability that prompt 63 does not specify, reply-matching rules
that can credit an old frame to new motion, incorrect V1.0.0 stop and mode assumptions, and first-contact experiments
that the required production API must refuse. **Verdict:** blocking issues. Ledger, codec and explicitly host-only
work can proceed after correcting the evidence and acceptance errors; device-facing registration should stay blocked
on a tested upstream owner contract; motion and commissioning need additional fail-closed rules for correlation,
identity, uncertain configuration changes and physical recipients. No implementation, test result or hardware
behavior was verified.

## Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *Prompt 63 as written cannot unblock MKS owner admission.* The dependency is on the right producer and its landed artifacts, but passing prompt 63's tests does not deliver 02b: they refuse restricted-range plans, and its single raw class does not separate an MKS stop from an ordinary MKS request. MKS needs the whole standard-ID space on an exclusive interface and a per-frame TX disposition; 02b blocks correctly, but reporting a request does not supply the deliverable. Have the S9 owner approve and schedule a tested extension (exclusive non-CANopen domain, stop precedence in it, per-frame TX disposition, restart and bus-off notification), keep the restricted-range refusals for shared interfaces, name its landed commit and consumer tests in 02b, and block only the dependent owner work until then. | 02b:28-30; 63:51, 59; shared README:29-31 |
| F2 | Critical, verified defect, high | *A new pending phase can accept the previous motion's completion.* A cancelled move A then a same-code move B: A's delayed status 2 matches every field of stage 09 and ends B's phase; likewise a duplicate after apparent completion. Quarantine starts only on timeout, and neither a host-declared quiet interval with no documented latency bound nor a controller restart proves the drive cannot still send the old frame; a host generation is not on the wire. Track ambiguity by address and reply shape (including echoed setting codes) across cancellation, timeout, duplicate, supersession and host restart; never promote an indistinguishable frame to fresh acknowledgement or completion without an evidenced retirement boundary; report accepted residual timing risk separately from qualified correlation; test an old 2 arriving during B, also after the quiet interval. | 09:16-20; review.md:278; AGENTS.md:79 |
| F3 | High, probable defect, high | *Stream state has no safe initialization or uncertain-setup transition.* 03 refuses a poll on an unknown stream, and 09 covers a stream known active across a restart, but not the state on first attachment to a used drive or after a possibly sent setup loses its reply; the dialect declaration does not establish stream state, and an acknowledged period-0 write does not retire a sample already queued. Define off, active and unknown transitions at first bind, rebind, possible setup or stop TX, lost reply and restart; default unestablished reportable codes to unknown; say how the application establishes off and retires residual samples; test a stream active before the host starts, a lost setup acknowledgement and a sample queued after report stop. | 03:51-52; 09:24, 27-29; review.md:174 |
| F4 | High, verified defect, high | *Urgent-stop handling breaks the one-outstanding-request invariant.* The contract permits one outstanding request per address plus a completion phase, yet 04 sends an urgent stop while a request is outstanding if the reply codes differ; distinct codes help matching but create no second slot, and the first request's reservation, deadline and terminal are unspecified, so the queued and in-flight tests have no expected transition. Choose one owner contract (for example retire a queued unsent start before the stop; for a possibly sent start keep its uncertain result and stop obligation, fence its reply, admit the stop as new intent), or revise invariant, capacity and matching for two slots; test that a retired start cannot transmit after the stop, also under queue pressure. | execution_contract:88; 04:29-31, 56-58 |
| F5 | High, verified defect, high | *The V1.0.0 motion and stop rules contradict the firmware history.* The review gives CR_CAN as V1.0.0's only bus mode and dates F7H to 1.0.4, yet 03 rejects every CR mode and 04 describes plain F7H on V1.0.0 as a low-current-limited stop rather than absent; 05 offers F5H live update without its 1.0.5 condition. The identity gate blocks V1.0.0 writes but does not make these codec, capability or stop assertions right. Drive mode admission, stop kinds and live updates from per-dialect, per-form ledger rows; test V1.0.0 CR_CAN as the bus mode independently of write authorization; refuse both F7H forms and F5H live update there; offer no motion that needs an unavailable interruption policy; keep the V1.0.0 identity gate. | 03:46-47; 04:18-19; 05:50-51; review.md:261, 265-266 |
| F6 | High, probable defect, high | *Declared and observed board codes must match, not just both be supported.* 03 accepts declarations for boards 2 and 4 and 08 validates current and I/O against the declared board; membership tests without an equality check let an observed 42D receive a request validated against the 57D's higher current limit, a case the negative tests miss. Require a fresh observed CAN interface and board code equal to the declared code before model-dependent preparation; in API and owner tests swap 2 and 4 both ways and assert no TX, no output change and invalidated model-dependent qualifications. | 03:9-11, 33-35; 08:18, 22 |
| F7 | High, probable defect, high | *A possibly applied bus-setting write lacks a defined quarantine.* 10 reserves both tuples and keeps execution unknown on a lost old-tuple acknowledgement, but not which admissions, queued work or cached qualifications stay usable between possible TX and confirmation, or after confirmation proves impossible; keeping the old binding as a record is necessary, using it as an established ID, check mode or reply policy is unsafe. Separate the retained binding from its eligibility; on possible TX suspend affected traffic and invalidate dependent qualifications at address or interface scope, on proven non-TX keep them; specify reservation lifetime and an operator reconciliation path; test a lost reply with the change applied and not applied, then an ordinary operation on both tuples. | 10:18-22, 28-30, 44-46; AGENTS.md:77 |
| F8 | High, probable defect, high | *A broadcast acts on the physical bus, not the application's member list.* 09 accounts for members in a node table, but an exclusive interface excludes CANopen, not an unlisted MKS drive, an old-dialect drive or an armed latch whose host record was lost, so 4BH can start motion outside the admitted set, and blanket invalidation of member phases is not reconciled with accepting a member's later completion after 4BH. Require a commissioned exhaustive physical participant list with compatible dialects, disjoint IDs and per-recipient authorization; for 4BH known latch state for every possible recipient, else refuse before TX; define what per-member phase may still qualify a later observation; test an incomplete table, mixed-dialect recipients and a lost latch record. | 09:40-43, 41, 47-50; 10:42-43 |
| F9 | High, verified defect, high | *First-contact questions require the production API to bypass its own gates.* Q5 needs the 4CH switch that 10 refuses until Q5 resolves it; Q6 needs reads under respond-off that 09 makes unavailable until Q6 resolves them; Q14 calls for malformed frames with no qualification-only path apart from the validated builders. Per question separate production observation from an exceptional experiment; keep normal admission closed; specify an independently reviewed, explicitly authorized procedure with bounded effects, capture and restoration, or use manufacturer evidence or an operator-controlled setup; otherwise mark the question not executable. | 12a:23-24, 33-34, 42; 10:26-27; 09:37-38 |
| F10 | High, verified defect, high | *Q6 and Q7 restore constants rather than the captured settings.* The procedure requires a snapshot and same-session restoration, but Q6 restores 8CH 01 01 and Q7 98H 0 without requiring those to be the initial values, which on a used drive can change completion reporting or leave host-silence protection disabled; a readback of the constant does not show restoration. Restore the exact captured value and dialect form and compare the final readback with the snapshot, or admit the question only when the pre-state equals the restore value; skip a write without a verified restore path; configure 98H while quiescent before the bounded run; rehearse with nondefault starting values. | 12a:20-21, 34, 35; 08:41-43 |
| F11 | High, verified defect, high | *Pre-restore FFH and 9AH values do not establish safe restart behavior after 3FH.* The 41H gate admits a restart when FFH and 9AH read off because parameters are unchanged; 10 applies the same gate to 3FH while listing FFH and 9AH in its unknown restore scope, so their values before 3FH do not establish those in effect at restart. Give 3FH its own gate: unless evidence establishes the post-restore power-on behavior for the specimen and dialect, require explicit acceptance of possible motion, a physically safe fixture and a feasible restore plan whatever the prior reads; test that off reads alone do not admit 3FH. | 10:35, 37-39; review.md:238-240 |
| F12 | Medium, verified defect, high | *Two prerequisite artifacts are defined after their first consumers.* 07 configures homing through the `SettingsContext` that 08 creates; 02b probes under a declared dialect whose binding-generation declaration and invalidation 03 introduces later, using a V1.0.0 probe key supposedly chosen in 01. Make the per-dialect probe an explicit 01 output and land the minimum validated declaration with 02/02b; move the settings context and homing descriptors before 07, or let 07 consume external homing settings and give the typed writes to 08; update README dependencies and acceptance lists together. | 07:11, 63; 08:9-10; 02b:45-48; 03:9-12 |
| F13 | High, probable defect, high | *Changing IN_1 to an output can invalidate a home or limit prerequisite.* 07 relies on IN_1 as home or left limit when not remapped; 08 lets 9FH turn it into an output on a 42D, but its invalidation list omits 9FH, and stationary admission does not make a later move safe while the application still treats the terminal as a limit. Check the proposed direction against the established home/limit assignment and wiring; refuse conflicting or unknown assignments; invalidate dependent input, limit and homing qualifications on possible application; test a 42D with IN_1 required, a separately qualified remapped configuration and a lost 9FH acknowledgement. | 07:42-45; 08:29, 46; AGENTS.md:85 |
| F14 | Medium, verified defect, high | *The exact speed conversion exceeds the review's qualified microstep scope.* 06 treats 16/MStep as exact for other powers of two and withholds only non-powers of two, while the review limits exact speed and acceleration mapping to 16/32/64 until other settings are measured, so executors can decide differently at 8 microsteps. Separate documented examples, an inferred formula and authorization for exact conversion; apply the review's narrower gate until the ledger records a scoped resolution; expose an unscaled native code; test a power of two outside the qualified set and a non-power of two. | 06:14-16; review.md:335 |
| F15 | Medium, verified defect, high | *Persistence policy is stated as device fact for 42H, and 0FH volatility is omitted.* Treating unknown writes as persistent is conservative but does not make the user ID documented persistent (the review lists 42H among commands whose persistence is not stated); the contract says only 83H has a volatile form, while the review also names MK109A's 0FH. Record separate evidence and policy fields (42H unknown, handled as persistent; 0FH documented volatile, write policy never); do not use a stored 42H as surviving-restart identity evidence; add checker cases preventing promotion of policy to fact. | 08:30; execution_contract:53-54; review.md:237 |
| F16 | Medium, verified defect, high | *The vector acceptance rule requires builders for forms that must never be built.* 01 collects every worked frame, 02 says every vendor example builds and parses byte-exact, yet excludes 0FH and 50H builders and keeps MK106 ledger-only; a well-formed forbidden or unsupported example is neither a builder test nor malformed. Give each vector a dialect, direction and use (positive builder/parser, observation-only, unavailable builder with unchanged output, malformed rejection); check source bytes and checksums independently of emission; apply byte-exact build acceptance only to supported transmissible forms. | 01:51-53; 02:35-36, 52-53; README:30-31 |
| F17 | Low, verified defect, high | *The family handoff uses a different completion token from the shared contract.* The family lists COMPLETE where the shared contract lists SOFTWARE COMPLETE; tools recording the literal token produce incompatible rows. Use `SOFTWARE COMPLETE` with the separate hardware column. | execution_contract:144-145; shared 02:139-140 |

## Unresolved questions (reviewer)

1. What establishes continuity of the drive session and coordinate origin when a drive can power-cycle without a
   bus-visible boot report? A new 40H read can agree with the old identity without proving the old origin survived;
   a decision is needed on which origin-dependent operations stay blocked; V1.0.0's lack of 40H is a separate gate.
2. At which boundary is local self-echo demonstrably excluded, and how will the test distinguish transport conformance
   from matcher behavior? A request byte-identical to its success reply cannot be told apart once delivered as RX.
3. Who resolves the shared generator naming decision and any proposed pre-S4 console exception? An informal decision
   to extend the Lichuan-typed application earlier does not replace a revised shared contract and its tests.
4. What specimen-specific evidence completes the first-power motor and wiring checklist? The review names a radial
   magnet, a phase-resistance condition and shielded twisted pair beyond 12A's shorter list; first power on a used
   drive may itself cause motion.

## Coverage gaps (reviewer)

Not supplied as file contents: the MKS manifest, original rendered PDF pages, extracts and schematics (device claims
were compared with the review and pack inventory only; hashes, DLC cells, worked vectors and schematic applicability
not checked); the owner, transport, core headers, CMake and package rules, verifier and tests; PROGRESS, landed seam
commits, prompts 55/57/64/65/73/74/75/77 and several convention, communication, architecture and CLI documents. No MKS
fixture, firmware observation, raw trace, timing bound or live-session authorization existed. The audit was text-only:
no checker, generator, build, CI job, package consumer or hardware test ran.

## Round 2

A condensed, faithful copy of the second report. It is untrusted reviewer input; the verified dispositions are in
[audit_application.md](audit_application.md) "Round 2". Line numbers refer to the files as revised by round 1
(committed in `e343591b`; the family files' hashes in the report equal those of that commit).

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-183325-8b7343` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used, reasoning effort high, depth standard ("large input (~161957 tokens)") |
| Date | 2026-11-19 18:33:25 (from the ID), 204.4 s, one attempt |
| Revision | `main` at `e343591bea01248818afee909443a93f2a500563`, uncommitted changes recorded (none in the audited files) |
| Cost | Estimated USD 0.35 (0.352848, configured pricing); 123,569 input and 10,571 output tokens (2,293 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-183325-8b7343.json` (local) |
| Location checks | 59 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_evidence_and_command_ledger.md` (131, `db9b188e`),
`02_codecs_and_dialects.md` (81, `498bf69e`), `02b_protocol_domain_and_owner.md` (128, `2fdbed8e`),
`03_reads_identity_and_dialect.md` (85, `62ab02e6`), `04_enable_stop_and_uncertainty.md` (73, `d231c1e2`),
`05_finite_position.md` (83, `a33f6970`), `06_speed_mode.md` (70, `f43508d9`), `07_homing_and_limits.md` (76,
`88a6ac07`), `08_settings_and_persistence.md` (71, `4642ceb5`), `09_async_reports_and_reply_matching.md` (99,
`94e07ea4`), `10_bus_settings_and_restart.md` (84, `11dc15bd`), `11_console_and_integration.md` (65, `bba026b6`),
`12_qualification_and_release.md` (63, `67d33c83`), `12a_first_contact.md` (72, `ccdf0a6b`), `README.md` (161,
`f9cfcdd2`), `audit.md` (82, `6f3db0a8`), `audit_application.md` (79, `f367fb61`), `execution_contract.md` (153,
`7ce68a22`). Requirements: `canopen_shared/README.md` and `02_profile_integration_contract.md` (before `d0108871`),
`ADD_DRIVE.md`, `COMMON.md`. Context: `docs/reference/mks_servo_can/README.md` and `review.md`,
`63_mixed_protocols.md`, `canopen_shared/01_existing_code_and_tooling.md`; project instructions `AGENTS.md`,
`CLAUDE.md`.

**Summary.** The revision makes sound corrections: owner traffic stays blocked on the tested S9 extension, the V1.0.0
mode and identity gates are separate, declared and observed board codes must agree, persistence policy is separate
from evidence, and factory restore has its own power-on-motion gate. The claim that every first-round finding is fully
resolved does not hold: reply attribution, stop ordering, multi-target admission and communication reconciliation
still leave consequential behavior unspecified or contradictory. Ledger, codec and host-only work can proceed after
the specification corrections; the affected device-facing paths should stay blocked. Text-only audit: no check,
build or hardware test ran. **Verdict:** blocking issues. The architecture (a separate proprietary backend beneath
common motion, prompt 63 recorded as an upstream dependency) is consistent; the blockers are in indistinguishable
replies, urgent stops, retained coordinate references, broadcast effects and uncertain configuration changes, which
decide whether the application can safely admit a command or claim its outcome.

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The quiet-interval rule gives the same late completion two outcomes.* 05 lets a status 2 in the ambiguous set complete a move under `QUIET_INTERVAL` and 09 lets that correlation end a phase, but 05's acceptance requires the old 2 not to complete B even after the interval; with no transaction ID a matcher cannot do both. Before the first observed duplicate there is no fence: waiting for a duplicate to fence its shape cannot protect a new request from it. Decide whether `QUIET_INTERVAL` is diagnostic only or a distinct risk-accepted terminal never treated as qualified; align 02b, 04-07 and 09 and their tests; define when a successfully completed exchange fences its shape and test A succeeding, B starting and A's first delayed duplicate during B on both sides of the interval; let a retirement boundary cover host-buffered RX as well as drive replies. | 05:42-44, 78-80; 09:16-18; 02b:71-75; AGENTS.md:79 |
| F2 | High, probable defect, high | *Bootstrap observations can be mistaken for fresh identity or a valid origin.* A possibly late or streamed 40H may show a responder, but 03 does not say which provenance makes identity "fresh" for the current binding and session, so a buffered frame from another binding could satisfy the board comparison. The README admits an uncommanded power cycle leaves the reference stale undetected, while 05 admits absolute moves on a host reference generation; an agreeing 40H does not prove the origin survived. Separate bootstrap observations from admission evidence, refuse dependent writes when only stale or possibly late evidence exists, default origin-dependent motion to blocked without established continuity pending an approved policy; test attachment or rebinding with a delayed 40H and an unchanged identity across a simulated reset with a changed origin. | 02b:54-56; 03:37-39, 63-64; 05:32-34; README:152-153; shared 02:65-70 |
| F3 | High, probable defect, high | *Freeing the matcher slot does not make the stop the last command transmitted.* In the possibly sent case 04 ends the start locally and admits the stop, but neither the S9 consumer gate nor prompt 63's request says whether a frame handed to the controller can still transmit after the stop; software queue precedence does not settle wire order, and no acceptance case asserts "no later start". Make retirement or ordering of every superseded changing frame an explicit S9 producer and consumer contract, including a handed-off frame of uncertain disposition; where the transport cannot prove it, keep the stop obligation and report the limitation; test a start held below the owner queue while a stop is admitted, under pressure and across bus-off or recovery. | 04:35-38, 66-68; 02b:32-34; 63:35-38 |
| F4 | High, verified defect, high | *Multi-target admission checks the code, not the frame's effect on every recipient.* 09 requires each recipient's dialect to document the code, but both V1.0.9 tables have F7H while only one has F7H 01; the acceptance refuses a mixed-dialect recipient that the work never defines; the generic dispatch does not exclude 4BH. Use a reviewed multi-target allowlist validating exact form, payload, check mode and authorized effect for every recipient before TX; state any outright mixed-dialect refusal in admission too; route 4BH only through its latch-aware operation; test a shared code with a one-dialect form and 4BH through the generic path. | 09:55-57, 67-70, 96-97; 02:38-39 |
| F5 | High, verified defect, high | *Communication quarantine can end without evidence of the active policy.* 10 step 2 allows three outcomes, but step 5 lets a declaration change end respond-off quarantine when confirmation is impossible, and a declaration does not distinguish an applied from an unapplied write; a response on the same slave ID cannot confirm an 8DH group change unless the read distinguishes old and new group values, on which group dispatch relies. Define qualifying evidence and permitted outcomes per setting (including what the menu can establish and what stays unknown); a declaration alone never restores admission or group membership; test applied and unapplied 8DH writes with identical probe results and an 8CH write without available confirmation. | 10:20-22, 30-32, 39-41; AGENTS.md:77 |
| F6 | High, probable defect, high | *Position at the target does not discharge a pending motion.* 05 uses a post-start 31H within tolerance as fallback completion with no minimum correlation, no proof that the latest trigger started and no rule for clearing the stop obligation; the shaft may be at the target without the intent executing (lost start reply, already in tolerance) or crossing the window while moving, and a streamed or delayed 31H weakens it further. Separate endpoint observed, latest intent completed and stop obligation discharged; require qualified evidence for the latter two, otherwise keep uncertainty and the bounded stop policy; test an in-tolerance target with a lost start, an in-motion crossing, a delayed 31H and an update whose latest target is not established. | 05:46-48, 51-53, 64-65; review.md:174 |
| F7 | Medium, verified defect, high | *The declared order still puts consumers before required artifacts.* 07 precedes 08 in the order and README table though it needs 08's `SettingsContext`; 05 and 06 can deliver motion before 07 defines limit admission and the homing-required flag; 09 needs participant validation that 10 is assigned. Put the settings context before 07, land a minimal limit-admission contract before motion or block earlier motion for enabled or unestablished limits, move participant validation to 09 with 10 reusing it, and update the table, handoffs and acceptance. | README:52-53; 08:9-11; 07:47-49; 10:57-58; COMMON.md:90 |
| F8 | High, verified defect, high | *The first-contact table promises routes before they exist or on dialects without them.* Q1 hardcodes 40H although V1.0.0 has another probe; Q2 and Q3 do not delimit reads per dialect; Q4 is scheduled after 05 but includes F6H work delivered by 06; Q7 needs 06's run-time route and, for the retained value, 10's restart; Q3 mixes reviewed bootstrap 00H queries with ordinary readback. Split questions into executable steps naming production operation, minimum stage, dialect and form, admission evidence and snapshot and restoration route; branch Q1 by the per-dialect probe, mark absent reads not applicable, separate Q4's F6H and Q7's restart parts; exploratory 00H without a reviewed route needs manufacturer evidence, the reviewed experiment path or NOT RUN. | 12a:8-9, 40, 42-43, 46; 02b:53-54; shared 02:189-192 |
| F9 | High, verified defect, high | *State-transition and deadline requirements lack executable acceptance.* The shared contract requires a source-state and intent table with exact first-frame or no-frame tests for enable, release, stop, fault clear and mode change, and a cumulative-delay test; 04 and 08 give encodings and some gates but not already-reached, unknown, mode and fault cases before the first TX, nor the cumulative-delay test. 06 calls its expiry stop new intent but never gives that stop's deadline or what remains pending if it cannot be admitted. Add the family tables and exact traffic tests with explicit refusal or uncertainty where evidence is missing, a multi-step case whose timely replies exhaust the deadline before the trigger, and for finite velocity the separate expiry stop's intent, deadline, admission failure and retained obligation without extending the expired run. | shared 02:111-119; 04:9-13; 08:25; 06:33-35 |
| F10 | Medium, verified defect, high | *Stop and interruption reply claims exceed the evidence.* 04 gives status 3 to every mode stop, but the review's F6H speed-0 row lists only 0, 1 and 2 (the position-stop row lists 3); 04 also says no frame reports an F7H interruption, while the review says the active move's reply after an emergency stop is undocumented. Interpret stop replies per code, form and phase from ledger rows; leave F6H stop status 3 unqualified; describe the interrupted move's response as undocumented, apart from F7H's own acknowledgement; test that an unexpected native status is retained without becoming a definitive stop or completion. | 04:27-28, 49-50; review.md:155, 313; AGENTS.md:71 |
| F11 | Medium, probable defect, high | *The fixture schema does not separate literal source errors from computed expectations.* 01 and 02 require every vendor vector's checksum to recompute, including malformed examples, RS485 bytes and wrong-DLC fixtures, but the review validated a set of MK109 examples only; a literal vector with a different or inapplicable checksum would be omitted or altered. Store original bytes, printed DLC and check byte apart from any derived expectation, give each a checksum domain and expected result (match, mismatch, not applicable) with discrepancy and source, keep vendor bytes and derived repairs as separate fixtures; replace "each motion code" in the stop inventory with the exact reviewed speed-zero forms so 91H and 4BH get no invented stop rows. | 01:47-48, 54-58; 02:58-60; review.md:62; execution_contract:45-46 |
| F12 | Medium, verified defect, high | *The pre-S4 console exception conflicts with the binding shared contract.* The execution contract blocks every console route and parity test until S4, but the README permits earlier delivery on a lead or user decision without a shared-contract change, though S4 is mandatory and a changed seam contract needs its callers and tests updated together; the README also calls an earlier shared revision binding while revising against round 2. Name the round-2 contract as the minimum baseline; keep every console route and parity case blocked before S4 unless an authorized shared-contract change lands with callers and tests; mark console parts of each stage's delivery and acceptance conditional. | README:100, 155-157; execution_contract:38-40; shared 02:123-124; shared README:40-42 |

### Unresolved questions (reviewer, round 2)

1. Which S9 extension work is approved and scheduled, and which landed owner and transport tests will prove
   retirement or ordering below the software queue? A recorded request is not a usable extension.
2. What session-continuity evidence or explicitly accepted residual risk may authorize origin-dependent motion, and
   will V1.0.0 stay reads-only?
3. What does a zero run-time field mean on the wire, and how is a positive finite duration that quantizes to zero
   handled?
4. How does the V1.0.0 39H observation represent the review's simultaneous signed int16 and 0..0xFFFF-per-turn
   descriptions, and how does its declaration represent a group setting that dialect lacks?
5. Which first-power facts must be established before energizing a used drive, and which may be learned from its menu
   after isolated power-up?
6. Which generator naming and output layout has the S7 owner approved for a command ledger?

### Coverage gaps (reviewer, round 2)

Not supplied as file contents: the pack manifest, rendered originals, extracts and schematics (hashes, printed DLC
cells, worked frames, electrical details and board-revision applicability not checked); owner and transport code,
receive timestamps, TX disposition, self-echo exclusion, core headers, checkers, CMake and package rules, workflows and
tests (the repository index was truncated); PROGRESS, landed seam evidence, prompts 55/57/64/65/73/74/75/77, the
conditional experiment prerequisites and several referenced contracts. The first round's audit and application were
read as summaries, not as the earlier file set and a diff. No fixture records, authorized session, CAN traffic,
latency, memory or service-gap measurements, compile results or exact-commit CI existed.

## Round 3

A condensed, faithful copy of the third report. It is untrusted reviewer input; the verified dispositions are in
[audit_application.md](audit_application.md) "Round 3". Line numbers refer to the files as revised by round 2
(committed in `c2a2882a`; the family files' hashes in the report equal those of that commit).

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-190731-c18295` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used, reasoning effort high, depth standard ("large input (~182969 tokens)") |
| Date | 2026-11-19 19:07:31 (from the ID), 215.8 s, one attempt |
| Revision | `main` at `c2a2882a8c7f950f07cfc18ff17c4870270f26b0`, uncommitted changes recorded (none in the audited files) |
| Cost | Estimated USD 0.38 (0.378422, configured pricing); 138,081 input and 10,226 output tokens (4,181 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-190731-c18295.json` (local) |
| Location checks | 33 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_evidence_and_command_ledger.md` (142, `bf3070a4`),
`02_codecs_and_dialects.md` (85, `5d873cf8`), `02b_protocol_domain_and_owner.md` (142, `e88aaeae`),
`03_reads_identity_and_dialect.md` (101, `eb61cd90`), `04_enable_stop_and_uncertainty.md` (110, `a8984a37`),
`05_finite_position.md` (104, `1699a7af`), `06_speed_mode.md` (77, `3cb874a1`), `07_homing_and_limits.md` (78,
`d62c299e`), `08_settings_and_persistence.md` (82, `a8cea1d5`), `09_async_reports_and_reply_matching.md` (108,
`692b73df`), `10_bus_settings_and_restart.md` (100, `3709993d`), `11_console_and_integration.md` (65, `bba026b6`),
`12_qualification_and_release.md` (63, `67d33c83`), `12a_first_contact.md` (75, `9587a1ef`), `README.md` (169,
`00e96db7`), `audit.md` (161, `c8e68451`), `audit_application.md` (155, `2b3909e9`), `execution_contract.md` (156,
`7cb489b9`). Context: `canopen_shared/README.md`, `02_profile_integration_contract.md` and
`01_existing_code_and_tooling.md`, `ADD_DRIVE.md`, `COMMON.md`, `docs/reference/mks_servo_can/README.md` and
`review.md`, `63_mixed_protocols.md`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The prompts correctly separate MKS from CANopen, keep owner traffic blocked on tested S9 artifacts and
distinguish software verification from hardware qualification, and several earlier fixes are present. The revised
rules can still discharge a motion obligation on mismatched observations, execute a latch without checking that its
original motion intent is still valid, or restore admission on evidence that proves only a responder; some early-stage
acceptance tests require artifacts assigned to later stages. Independent ledger, codec and host-only work can proceed;
the affected device-facing paths need corrected contracts and tests before admission. **Verdict:** blocking issues.
The defects decide whether motion may start, whether a stop is still owed and whether a changed binding may transmit
again; they are specification defects, not observed device failures. A recorded S9 request is not a landed owner
capability, and nothing supplied establishes implementation or hardware qualification.

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *A position sampled in motion can still discharge the stop obligation.* 05's fallback combines an earlier in-tolerance 31H sample with later F1H and 32H observations; an acknowledged move can cross the window and stop elsewhere (also after a stall or host-silence event, which send no frame), so every predicate passes without the drive shown stopped at the target, defeating the in-motion-crossing test. Require the latest target within tolerance in the same standstill observation window, with ordering, freshness and invalidators for the position and stop-state samples; otherwise leave completion unknown and keep the stop obligation; test an acknowledged start, an in-tolerance crossing and fresh stopped reports after the axis left the window. | 05:62-64, 99-101; 04:77-79 |
| F2 | High, verified defect, high | *The latch trigger has no live-intent gate and no consistent completion rule.* 09 checks participant exhaustiveness and latch history, but a known status-5 latch can outlive local cancellation or its deadline with no evidence that either clears it, so a new 4BH could start an expired or cancelled motion. 09 also requires a latched member's later completion under 05, which needs an attributed status 1 that the staging reply 5 and the reply-less 4BH never supply. Define per-member latch states separating a known remote latch from a live, authorized intent; verify every affected intent and deadline before 4BH or refuse; define the state after possible TX, cancellation, a stop and a second 4BH without assuming the latch cleared; say whether an attributed 2 is only an observation or completes through a distinct rule; test a known but expired or cancelled latch, an uncertain first 4BH and completion without an invented status 1. | 09:68-69, 73-75, 106; 05:61-62 |
| F3 | High, probable defect, high | *A retained stop obligation does not explicitly bar conflicting new work.* 04 admits an urgent stop without retirement proof and frees the start's slot, but the release row checks only for an admitted motion or pending phase, so a stopped F1H taken before a late start can satisfy it; 08's stationary declaration and no-admitted-motion check have the same gap for settings. Make a retained motion obligation or unretired start a per-address barrier for release, conflicting writes and unrelated new motion, keeping urgent stops and observations; define when it clears from final TX or retirement evidence and the motion or stop outcome, across recovery; extend the late-start test with a release and a mode or scale change after the first stop returned. | 04:24, 63-65; 08:44-45 |
| F4 | High, verified defect, high | *Communication quarantine can be lifted on probe evidence that expressly grants no admission.* 10 accepts a probe-grade responder to confirm a new ID, bitrate or check mode, while 02b lets a `possiblyLate` frame meet that standard and says it never stands as admission evidence; even a current probe reply shows a responder, not the commissioned drive or the applied change, and only the group-ID row requires attributed readback. Separate discovery from reconciliation evidence: a deciding attributed readback or specimen-specific operator evidence distinguishing old and new configuration, with continuity to the intended drive when the tuple changes; otherwise keep quarantine or record the node lost; test a `possiblyLate` probe, an unrelated responder on the new tuple and a reply compatible with both check modes. | 10:40, 43; 02b:59-60 |
| F5 | High, probable defect, high | *The identity-read exception can qualify an indistinguishable stream sample.* 03 allows 40H as a bootstrap observation with stream state unknown, but its admission rule only adds a handoff and `QUALIFIED` or `QUIET_INTERVAL` correlation, while 09 treats any read-only code as possibly reportable and samples share the polled shape; a sample captured after handoff can satisfy the matcher without answering the read, and a quiet interval does not show that a stream stopped, so model-dependent current or I/O work could be authorized. Keep the exception for raw bootstrap observations; before 40H is admission evidence require it established non-reportable, its stream established off with residual traffic accounted for, or another discriminator; test a matching sample with `ACTIVE` and `UNKNOWN` state after handoff, also after the interval, with no dependent changing TX. | 03:46-47, 74-75; 09:32-33; AGENTS.md:79 |
| F6 | High, verified defect, high | *The mode-change table cannot commission a V1.0.9 drive from its documented default mode.* 08 requires F1H 1 for every changing 82H row, including an unknown prior mode, while 03 says F1H reports only calibration in the V1.0.9 CR_* modes and the review gives CR_vFOC as the default, so the typed route cannot establish its stopped state on a factory-default drive and the table neither marks that source as an intended refusal nor offers another route. Make the table explicit by source mode; without reviewed non-bus-mode evidence specify zero changing TX and an operator-established bus mode before software commissioning, documented and tested; never let a fake drive supply F1H 1 in CR_* to exercise the positive row. | 08:52; 03:35-36; review.md:185 |
| F7 | High, probable defect, high | *The replacement limit gate does not say what happens when limit configuration is unknown.* Before 07, 05 and 06 refuse on enabled or unestablished EndLimit; 07 replaces that gate but refuses only known-enabled limits awaiting homing, and defines neither admission for an unknown or possibly changed setting nor what credits a newly attached binding with earlier limit homing, so an implementation could admit motion the interim rule refused; 08 already treats a possibly applied I/O direction change as invalidating. Make it a fail-closed per-binding decision (established off; on with current wiring, configuration and homing evidence; unknown or invalidated), define when a possibly applied change invalidates it and which homing route satisfies it; test first attachment, a lost homing or limit write reply and an invalidated input assignment with zero ordinary motion TX until requalified. | 05:6-7; 07:48-49; 08:58-59 |
| F8 | High, verified defect, high | *Early-stage acceptance still depends on later stop, stream and settings work.* 03 requires a stop admitted on contradicting identity before 04 creates stops; 03 starts potentially reportable reads `UNKNOWN` and only adds the refusal, while 09 first defines how to establish `OFF`, so 04's fresh 3AH, 3EH and F1H reads and 05's 31H reachability read have no delivered path (injecting `OFF` into the fake proves nothing); Q9 names 05 as its minimum stage though its 95H configuration comes from 08 with a snapshot and restore. Land the minimal generation-bound stream-state record and its evidence-backed `OFF` declaration with 03, leaving report writes in 09; put the real stop-under-identity-refusal test in 04 and test 03's gate without a stop stub; split Q9's read-only cases from the 95H-changing ones, naming 08, the snapshot and the restore route. | 03:71-72, 96-97; 04:90; 09:38; 12a:51; 08:28 |
| F9 | Medium, verified defect, high | *The family excludes S3 wholesale even if it owns the first S2 migration.* MKS rightly neither supplies nor uses a CANopen best-effort stop shape, but the shared README and contract require a default-deny change in the common owner in the S2 commit, before any further family's first binding; the MKS README calls S3 not applicable and 02b's first-S2 migration omits that owner-side change, and the absence of an SDO route is no test of the owner's default. Separate the common S2 default-deny prerequisite from an MKS opt-in, which stays absent; if MKS lands S2 first, name the owner change and its no-TX regression in the S2 handoff; if tested S9 isolation should satisfy it for proprietary profiles, the shared contract, callers and tests change together. | README:119; 02b:47; shared README:54; shared 02:309 |

### Unresolved questions (reviewer, round 3)

1. Which approved, landed S9 commit and consumer tests deliver exclusive MKS admission, per-frame TX disposition,
   retirement evidence, capture-time bounds and invalidation notifications? Prompt 63 records the request and some
   exclusive-domain tests, which is no evidence that the extension is approved or usable; 02b rightly keeps dependent
   owner work blocked.
2. Which fixture-specific motion and timing limits will distinguish a passed physical qualification from a retained
   observation in 12B? The stage names operations and evidence, but a procedure must define the motion envelope, stop
   and service timing criteria and abort conditions before observations can support a named qualification claim.

### Coverage gaps (reviewer, round 3)

Not supplied as file contents: the MKS manifest, rendered pages, extracts and schematics (device claims were compared
with the review only); owner and transport source, TX retirement and RX timestamp contracts, TWAI tests, landed seam
records, checker and package source and CI workflows (the repository index was truncated); main-sequence prompts other
than 63, the conditional experiment prerequisites, parent project instructions and several interface and bench
contracts. No fixture record, raw trace, reply-latency bound, stop measurement or service-gap data existed; the audit
was text-only, and no checker, build, CI job or hardware test ran.

## Round 4

A condensed, faithful copy of the fourth report. It is untrusted reviewer input; the verified dispositions are in
[audit_application.md](audit_application.md) "Round 4". Line numbers refer to the files as revised by round 3
(committed in `ed32d654`; the family files' hashes in the report equal those of that commit).

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-192905-1c8982` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used, reasoning effort high, depth standard ("large input (~198187 tokens)") |
| Date | 2026-11-19 19:29:05 (from the ID), 206.4 s, one attempt |
| Revision | `main` at `ed32d6541be8ee96e487fdd05b9884256e3f73dd`, no uncommitted changes recorded |
| Cost | Estimated USD 0.41 (0.408278, configured pricing); 148,219 input and 11,184 output tokens (5,032 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-192905-1c8982.json` (local) |
| Location checks | 37 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_evidence_and_command_ledger.md` (142, `a357bac7`),
`02_codecs_and_dialects.md` (85, `5d873cf8`), `02b_protocol_domain_and_owner.md` (147, `5f0750d6`),
`03_reads_identity_and_dialect.md` (112, `7b777885`), `04_enable_stop_and_uncertainty.md` (122, `e0829687`),
`05_finite_position.md` (113, `e34d3c90`), `06_speed_mode.md` (77, `3cb874a1`), `07_homing_and_limits.md` (88,
`55f34261`), `08_settings_and_persistence.md` (91, `6266008d`), `09_async_reports_and_reply_matching.md` (130,
`ca6927a4`), `10_bus_settings_and_restart.md` (103, `7453cc3a`), `11_console_and_integration.md` (65, `23e4111a`),
`12_qualification_and_release.md` (63, `67d33c83`), `12a_first_contact.md` (76, `ca009de7`), `README.md` (171,
`317596d0`), `audit.md` (230, `fdb0be95`), `audit_application.md` (231, `22d505a0`), `execution_contract.md` (156,
`7cb489b9`). Context: `canopen_shared/README.md`, `02_profile_integration_contract.md` and
`01_existing_code_and_tooling.md`, `ADD_DRIVE.md`, `COMMON.md`, `docs/reference/mks_servo_can/README.md` and
`review.md`, `63_mixed_protocols.md` (80 lines, as of `ed32d654`, before `c67e48dd`); project instructions
`AGENTS.md`, `CLAUDE.md`.

**Summary.** The prompts correctly isolate MKS from CANopen, gate owner traffic on landed and tested S9 artifacts and
distinguish a responder observation from evidence that authorizes work; several earlier corrections hold (the
stream-state prerequisite, the board-code equality check, the settings-before-homing order). The remaining blockers
are in how the stages compose: the motion barrier conflicts with live operations, stop and latch completion have
incompatible evidence rules, and stream and communication recovery can restore admission without establishing the
state they rely on. Text-only review; no implementation, CI, vendor original or hardware behavior was verified.
**Verdict:** blocking issues. Independent ledger, codec and host-only work can proceed; the affected device-facing
motion, report and communication paths need one executable admission and completion contract with regression tests,
and owner-backed delivery stays gated on the tested S9 producer artifacts.

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The retained-stop barrier conflicts with live motion transitions and omits normal terminal paths.* 04 refuses every changing operation except a stop while a start may still transmit or a stop obligation is outstanding, without distinguishing a live operation from a failed or preempted one, yet 05 offers an F5H update on an admitted move and 09 a 4BH for live latched members with no way past the barrier; conversely 06 ends a run on run-time completion and 07 recognizes homing completion, while 04 names only 05's position completion or a new stop as discharge. Define one per-address transition table separating live intent, pending or unretired TX, uncertain execution and a retained obligation after local termination; the exact conditions for an update or latch trigger to belong to the same live intent; and the terminal evidence that discharges each motion type. Keep per-drive latch-risk acceptance separate from the TX-order barrier. Test a valid live transition, the same with an unretired predecessor, and finite-velocity and homing completion followed by new admission. | 04:71-73, 75-77; 05:76-79; 06:50-52; 09:88-89 |
| F2 | High, verified defect, high | *A same-code status 2 is both cause-indistinguishable and eligible to discharge an ordered stop.* 04 says status 2 after a same-code stop cannot tell a reached target from a completed stop, then lets an attributed stop-complete reply clear the barrier without excluding that shared status; 02b lets an ambiguous-shape frame end a phase after the quiet interval. A run's 2 after a speed-0 stop can therefore satisfy the discharge rule; TX retirement proves wire order, not which operation produced the reply. State whether such a 2 may clear the barrier; under the stated policy it stays an observation even with quiet-interval correlation, discharge using the fresh post-stop observation route. Test an attributed run 2 after the stop's handoff with the stop's reply withheld, and the obligation and a following changing request. | 04:51-53, 76-78; 02b:101-103 |
| F3 | High, verified defect, high | *The latch completion test requires a window whose prerequisites cannot be met.* 09 completes a latched move only through 05's standstill window after 4BH and forbids inventing a status 1, but the window's `endpointObserved` is defined from a read after the latest trigger's acknowledgement and `completed` needs that trigger's status 1; the staging 5 acknowledges holding a command, not executing the reply-less 4BH. Dropping the check would let an axis already in tolerance look complete after a 4BH that never arrived; 05 withholds endpoint proof for pulse moves and an F6H run has no position target. Specify latch observations and terminal rules per native form (staging accepted, trigger possibly sent, endpoint or standstill observed), keep execution unknown unless evidence shows this trigger executed, and mark forms without a reviewed route. Test a live latch whose trigger is not delivered and a real post-trigger endpoint observation, including a target initially in tolerance. | 09:93-96, 126-128; 05:60-64, 70-73 |
| F4 | High, probable defect, high | *A risk-accepted report-stop reply can turn an active stream into authoritative polled evidence.* 09 promotes an acknowledged period-0 stop to `OFF` without a minimum correlation, while setup and stop share one reply layout; a delayed setup success can end a later stop as `QUIET_INTERVAL` although the stop did not apply, and the still-active stream's samples, identical in form to polled replies, then need not be classified as unsolicited, defeating 03's stream guard (40H too, if reportable). Do not infer `OFF` from a quiet-interval acknowledgement when an earlier setup can produce the same reply; keep `UNKNOWN` until a reviewed discriminator or the caller's `OFF` declaration, retaining the fence for residual samples. Test an applied setup, a stop that does not apply, a delayed setup success after the interval and a sample during a later poll, with no dependent changing TX. | 09:27-28, 37-41; 03:46-49; AGENTS.md:79 |
| F5 | High, probable defect, high | *Communication quarantine can clear before an uncertain change is settled.* 10 quarantines on possible TX but lets an attributed old-value read or a menu reading restore the old binding, while 02b allows a disposition that does not exclude later transmission, so a write held below the owner queue could apply after a valid old-value observation and after traffic resumes; even with final TX evidence, activation timing is undocumented, so a current value does not rule out a deferred change. 10 also invalidates facts after 3FH without defining reservations and admission during its unknown outcome. Separate local TX settlement from configuration settlement, fence the scope before the frame can be handed off and while it could still transmit, and require a reviewed settlement or operator reconciliation boundary before a value restores traffic; give 3FH a quarantine and participant-record rule, including what "node lost" permits on the interface. Test a delayed handed-off write after an old-value read and a restore with an unexpected tuple, neither allowing ordinary or multi-target TX. | 02b:34-37; 10:18-20, 31-34, 60-63; AGENTS.md:77 |
| F6 | High, probable defect, high | *Lost mode and microstep replies lack a required invalidation and queue test.* 08 lists what 82H and 84H invalidate but not when, nor how dependent queued work retires; only 9FH covers a possibly applied change with a lost reply, and the general lost-reply test asks only for unknown execution and no repeat. Tied to acknowledgement or readback, an applied 84H with a lost reply leaves a stale pulse scale (a different displacement) and an applied 82H stale work-mode evidence. Require effect-specific invalidation for any write that may have applied, before dependent work is handed off, keeping old qualifications only on proven non-transmission or other deciding evidence; say how handed-off dependent work is reported; test acknowledged, possibly applied with no reply and proven unsent 82H and 84H writes against retained evidence and queued motion. | 08:61-65, 83-85; 05:21-23; AGENTS.md:77 |
| F7 | Medium, verified defect, high | *Stage 04 still requires an end-to-end refusal test for writes introduced in stage 08.* The README fixes the settings-before-homing order but not 04's new case: after a late start and an apparent stop, a mode or microstep write must be refused through the real path, while the typed settings arrive in 08; a direct guard call or stub cannot show the later route enforces the barrier. Keep the late-start and release regression in 04, move the mode and microstep regressions to 08 through its real settings route with 04's barrier as a prerequisite, and keep any serialized stage order aligned with the README. | 04:111-114; 08:9-11; README:62 |
| F8 | Medium, verified defect, high | *Prompt 63's exit criteria do not establish the complete S9 interface that 02b consumes.* Prompt 63's plan and raw-admission items still describe restricted-range validation and a raw class below stops without an explicit exclusive-domain branch; its exit list does not require whole-space admission on an empty interface, refusal in both CANopen/MKS admission orders or bus-off delivery to the exclusive service, and names RX capture times without the TX handoff time or ordering evidence 02b's matcher needs. A producer acceptance gap; 02b correctly stays blocked without the artifacts. Make shared and exclusive cases explicit in prompt 63, name per-frame TX handoff and RX ordering evidence with uncertain bounds, add producer tests, keep the shared-interface negatives, and have 02b rerun the named tests from the landed commit. | 63:33-41, 56-62, 69-73; 02b:71-73, 141-143 |
| F9 | Medium, verified defect, high | *Physical release qualification has no required fixture-specific pass criteria.* 12B lists motion, stop, homing and timing cases with stop prerequisites and restoration, but no predeclared motion envelope, measurable stop or service limit, abort boundary or criteria separating a pass from a recorded run; round 3's application names the gap and calls such runs observations, yet the stage still allows a qualification disposition without the procedure. Require a reviewed fixture-specific procedure before a result can count as hardware-qualified, naming per case the motion envelope, stop and service timing criteria and their measurement, the abort action and the expected end and restoration state; otherwise keep the trace as an observation and the row open. | 12B:33-38, 59-62; audit_application:190-195 |

### Unresolved questions (reviewer, round 4)

1. Which landed S9 commit and consumer tests demonstrate exclusive admission, ordered or explicitly uncertain TX
   retirement, comparable RX capture and TX handoff evidence, and restart and bus-off delivery? The prompt records
   requirements, not a usable owner capability; only dependent work should stay blocked.
2. Which specimen-specific evidence can establish communication settlement after a possibly applied write or
   restore, rather than merely a current menu value or a current responder? Without a deciding boundary, ending
   quarantine can restore access to an unsettled address or interface.
3. Is V1.0.0 intended to remain reads-only, and what evidence will resolve the continuity policy for absolute motion?
   Neither a dialect declaration nor a repeated 40H observation establishes the missing identity or origin
   continuity.

### Coverage gaps (reviewer, round 4)

Not supplied as file contents: the MKS manifest, rendered manuals, extracts, schematics and original worked vectors
(device claims compared with the review only; DLC cells, checksums, electrical limits and board revisions not
checked); owner and adapter sources, TX retirement, RX timestamp semantics, self-echo conformance tests, seam commits
and PROGRESS (the repository index was truncated); parent instructions, prompts 55, 57, 64, 65, 73, 74, 75 and 77,
the experiment prerequisites, and console, build, package and CI definitions. No fixture record, CAN trace, latency
bound, stop or service-gap measurement existed; no checker, build, test, CI job or hardware procedure ran.
