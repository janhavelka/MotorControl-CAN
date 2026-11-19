# Independent audit of the Lichuan C-family extensions promptset, 19 November 2026

A condensed, faithful copy of the reviewer's report. It is untrusted reviewer input; the
verified dispositions are in [audit_application.md](audit_application.md). Line numbers
refer to the files at the audited revision, before the revision that applied the findings.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-180845-a43848` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-astra` requested and used, reasoning effort xhigh, depth complex ("focus mentions safety-critical control") |
| Date | 2026-11-19 18:08:45 (from the ID), 328.0 s, one attempt |
| Revision | `main` at `1043608bd0f58c5b371877424d2c783f9ed33e72`, no uncommitted changes recorded |
| Cost | Estimated USD 1.49 (1.491016, configured pricing); 108,162 input and 15,643 output tokens (7,916 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-180845-a43848.json` (local) |
| Location checks | 56 verified, 0 relocated, unverified or invalid |

Promptset under audit: this directory's `README.md` (127 lines, sha256 `313a8cc5...`),
`execution_contract.md` (143, `89f7a0e4...`), `01_models_and_identity.md` (104, `accf813e...`),
`02_model_ledger.md` (88, `0c736ff2...`), `03a_tlc_rows.md` (80, `667b6817...`),
`03b_tlc_operations.md` (86, `b1bdb3d7...`), `04a_cl2_rows.md` (81, `16895f19...`),
`04b_cl2_operations.md` (85, `77b2be48...`), `05_en57_conflicts.md` (62, `6bf1594a...`),
`06_console_and_coverage.md` (67, `38eda377...`), `07a_first_contact.md` (68, `ba08ad1f...`),
`07b_qualification_and_release.md` (67, `711bcae1...`). Requirements: `canopen_shared/README.md`,
`canopen_shared/02_profile_integration_contract.md`, `ADD_DRIVE.md`, `COMMON.md`. Context: the LX pack
`README.md` and `review.md`, `57_regular_drive_operations.md`, `docs/reference/lichuan_review.md`; project
instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** Keeping one `LICHUAN_C` profile and separating documentary, host-tested and
physical evidence is sound, and several device decisions follow the delta review (CL2
`2501` stays raw and unwritable, CL2 bit 10 is not zero speed, EN57 adds no 57C
capability). The set is not ready for independent execution: firmware admission conflicts
with prompt 57, several first-contact measurements require the evidence they are meant to
obtain, and model-local checks do not cover all shared-bus effects. **Verdict:** blocking
issues. The documentary and fail-closed scaffolding can proceed; the motion, shared-interface
and first-contact stages need corrected admission rules and executable evidence paths, and
some acceptance criteria would fail a correct implementation because they prohibit reads the
same stage requires.

## Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The operation stages reinstate the exact-firmware lock that prompt 57 removes.* Prompt 57 says a firmware difference alone must not refuse motion, but 03b and 04b require evidence records whose firmware equals the current identity, moves and velocity included; a correct common identity check would still be followed by a refusal. Removing all evidence checks would instead wrongly transfer a specimen's stop, trajectory or scale qualification. Separate the product-mismatch refusal, the model-identity outcome, the firmware-unqualified report and each operation's behavioral prerequisite; define the behavior before and after prompt 57 once; add paired tests where only firmware differs and where a required fact is absent, the latter naming its gate rather than disguising a firmware lock. | 03b:14-16; 04b:16; 57:72-74, 92 |
| F2 | High, verified defect, high | *Confirmation against a finite specimen set is overstated as protection against every misdeclared TL-C.* The proposed rule for a future C-side filter writer is stronger than checking `1018`, but an unmeasured TL-C can match the C reference and differ from the supplied TL-C references, so prompt 57 would report CONFIRMED for the C declaration. No C filter writer exists today; the defect is the proposed criterion. The README groups `2000` with values shared by all three families, while the delta establishes that default only for TLEDS against C-EDS. Existing C admission without an identity check is a compatibility choice, not evidence the legacy declaration is right. Describe CONFIRMED as unique among supplied references; keep the hazardous writer blocked without independent model and binding evidence for the deployment; document the residual wrong-declaration risk of existing C operations (persistence included) without silently changing prompt 57's vendor-write policy; split the three-EDS `1000`/vendor/product claim from the two-EDS `2000` claim; test missing, equal and apparently unique references with exact reasons. | 03a:45-47; 57:84-85; 01:64-66; README:98-100; LX review:112-114 |
| F3 | High, verified defect, high | *07a cannot obtain several first measurements through the production routes it requires.* T6 asks for a run per velocity order but `prepareVelocity` needs a TL-C record that already fixes the order; T5 proposes `0x010F` as third enable word, which 03b forbids. The first-measurement plan exception covers a missing stop predecessor, not missing enable, move or velocity evidence, and conflicts with 03b's unconditional stop-before-motion test; CL2 K4/K7 have the same bootstrap problem. 07a lets an X gate change when evidence is complete, while the shared contract says first contact never opens a gate. Give each question an admitted production route with prerequisites, an applicable manufacturer-evidence path, or a separately assigned, independently reviewed isolated experiment (permitted actions, observation, snapshot, cutoff, restoration, failure boundary), otherwise NOT RUN; remove the bypass from production admission; let 07a attach observations only and a later reviewed model/firmware-scoped change open a gate. | 03b:20-22, 26-29, 39-41; 07a:37, 60-62; canopen_shared 02:189-193 |
| F4 | High, verified defect, high | *A per-model `sync` refusal does not protect a mixed interface before S13.* 06 calls for CL86-C, TL-C and CL2 on one interface; refusing SYNC through a new model's API does not stop the interface producer, including a schedule armed for the C node before the new binding appeared, and the shared inventory says the pre-S13 producer lacks a population-wide check. Distinct node IDs and one bitrate do not establish compatible COB-IDs or acceptable NMT broadcast effects; CL2 communication records are undocumented, and a nominal three-node fake test cannot establish these. Make the positive shared-interface case conditional on a recorded identifier, routing and broadcast analysis; before S13 require isolation or a configuration where SYNC cannot be emitted while an unknown-effect node is present, after S13 consume its guard; test adding or rebinding the new model with a C-originated schedule active, unrelated-target SYNC, conflicting routes and broadcast NMT, asserting interface traffic. | 06:36-37; README:125-127; canopen_shared README:64; canopen_shared 02:153-154; AGENTS.md:59 |
| F5 | High, verified defect, high | *04a both exercises and forbids an upload of undocumented CL2 identity fields.* `1018` is undocumented, the identity route and its test handle an abort or a value from an optional `1018` request, yet acceptance forbids planning any read of an undocumented object. K3 requests more undocumented communication records and K9 includes `1001`, absent from 04a's documented list; accepting a read risk does not create a production row. Choose an explicit policy for CL2 `1018` (a narrowly reviewed caller-selected bootstrap upload with raw reply/abort semantics, or no production request and a separate first-contact path); enumerate other permitted probes by index, subindex and route; test exact uploads with the option off and on, no marker, retry or download, and no model inferred from an abort; keep K3/K9 outside ordinary acquisition unless individually admitted. | 04a:14-15, 20-21, 78-80; 07a:49, 55 |
| F6 | Medium, verified defect, high | *03a's zero-TX criterion prohibits the raw TL-C reads needed for safe persistence.* An SDO upload request is transmitted traffic too. The work permits raw observations and PDO map readback, but deliverable and acceptance say no TX to those indices or a PDO record, which fails a correct read implementation and makes 03b's save preflight impossible. State the oracle by direction and intent: approved uploads have exactly their reviewed request frames; no download to `2401..2407` or `2203` is planned for TL-C; a refused setter, restore plan or map mutation emits nothing; likewise for PDO communication and map readback. | 03a:33-35, 54-56, 77-79; 03b:61-62 |
| F7 | High, verified defect, high | *The CL2 index-homing admission is weaker than the fixture prerequisite.* 04b admits index methods with a caller-declared index signal; its negative test covers only an absent declaration, not a declaration on an absolute-encoder CL2 with no documented Z source, which K7 says needs a documented source first. A declaration describes wiring; it cannot show the drive variant exposes or uses Z, and seeking an unavailable index is not a bounded move. Split admission by model and signal set: refuse index methods on CL2-57C/86C even with a declaration until source evidence and qualification exist; on ABZ require Z actually available on the fixture; test both, the method-specific switch and start-direction prerequisites and refusal before setup or trigger; keep the CL2 trajectories and the method-35 refusal. | 04b:40-42, 76-77; 07a:53; LX review:487-490 |
| F8 | High, verified defect, high | *The save tests assert route stability that the evidence does not establish.* 04b infers no CL2 route change after a save because bitrate is DIP-selected, but a route includes the node, the stored `2203` relation to the unlisted all-OFF setting is unknown and no complete persistent list exists. For TL-C, T10 says bitrate stays unchanged unless a selector experiment runs, though a previously staged selection can become effective at save and restart without a library write. Separate "this operation sent no node or bitrate write" from "the device will answer on this route after restart"; state verified DIP/fixture prerequisites; for save and factory restore keep possible route effects on success or lost acknowledgement, invalidate dependent evidence on restart and require explicit reacquisition without replay; test the uncertainty instead of making the fake server's unchanged route a guarantee. | 04b:60-63, 78; LX review:507-508; 07a:41; 03b:59-60 |
| F9 | High, probable defect, high | *The completion paths lack an admitted way to acquire fresh, model-correct status evidence.* CL2 needs a fresh low-to-high bit 10 in the commanded and displayed mode, but CL2 uploads may not be polled without a new decision and no CL2 PDO route is qualified; repeated SDO samples hit the read-effect prerequisite. `6061` and `6041` from one acquisition are not an atomic sample across a mode change or reset. For TL-C the pairing check applies only when a model is supplied, the legacy getter keeps C semantics, and Doxygen alone does not stop an operation from feeding it a new-model observation. Specify the admitted observation source for each completion and stationary predicate (position clear included); bind samples to model, operation/trigger, mode and generations; let no legacy getter or mismatched binding establish a motion fact; require the model's read acceptance before polling or leave completion UNKNOWN; test pre-high, mode change between reads, stale or mixed samples, missing transition, fault and expiry under the one operation deadline, with no inferred standstill or later trigger. | 04a:26-27, 33-35; 04b:29-31, 50; 03a:27-28, 40-41 |
| F10 | High, probable defect, medium | *A preparation-time copy of an identity outcome can outlive its authority to send.* 01 proposes carrying the outcome in `AxisReservation` and checking it at preparation. A retained copy in an operation or queued transfer can survive a reset, configuration change or contradicting read before a later download; a model-enum check does not tell two sessions of the same node apart, and the listed stale-generation preparation test does not show retirement at dispatch. Specify the owner of current binding, session and configuration evidence and how invalidation reaches prepared, queued and partly executed operations; keep an admission snapshot for reporting, not as TX permission; test invalidation between prepare and dispatch and between two changing steps, including a new CONTRADICTED outcome on the same model, asserting no further prohibited TX, untouched unrelated nodes and uncertainty for frames already handed off. | 01:44-46, 67-69, 91-93; AGENTS.md:77 |
| F11 | Medium, verified defect, high | *The ledger cannot express documentary-only 57C applicability with the required model keys.* 02 defines each manual row's `models` as `SettingsModel` values, but EN57 rows document CL57-C and OL57-C, for which 01 and 05 forbid model values; an empty list loses the applicability, and UNKNOWN or an 86C value misattributes it, also for existing 57C contrast evidence. Use documentary designation keys separately from runtime keys with an optional mapping; generate admission only for mapped, implemented rows; add a checker case where an EN57 row names both 57C products, counts for documentary coverage and generates no runtime selector or write admission. | 02:36-37; 01:29; 05:8-10 |
| F12 | Medium, verified defect, high | *Not every required refusal has a defined X gate or coverage disposition.* 03b requires each refused operation to report its X gate and 06 the console to print it, but the X table has no TL-C gate for a missing stop predecessor, relative basis or scale, method-specific homing evidence or the persistence obligation. 06 lists release and mode-select rows without saying which disposition applies when prompt 57 is absent or its CL86-C release evidence does not transfer. Absent function, missing caller evidence and unresolved device conflict are different reasons. Let 02 produce a total operation-by-model disposition map before the operation stages; assign a gate and evidence owner for qualification questions and NOT_DOCUMENTED, NOT_IMPLEMENTED or missing-prerequisite reasons otherwise; define release and mode-select dispositions for both landing orders of prompt 57; drive API/CLI checks from the map without equating a documented entry with an admitted operation. | 02:56, 62; 03b:84-85; 06:42-45, 62-63 |

## Unresolved questions

1. Which fields and presence/width rules make a model-reference match, as distinct from
   product refusal, firmware reporting and specimen provenance? An unread CL2 `1018`, an
   optional serial, a width mismatch or a firmware-only difference must classify
   deterministically, and UNCONFIRMED is not positive evidence for every operation.
2. What binding entry point guarantees `SdoReusePolicy::NONE` before the first TL-C or CL2
   transfer, including use outside the example application and after rebinding? The owner
   default stays Lichuan-wide, so a timeout can trigger an automatic marker read on a model
   whose read risk was never accepted.
3. Is OL86-C an authorized bench-script target or only a supported legacy API model? 06
   allows both CL86-C and OL86-C in the pre-port guard while COMMON identifies the attached
   drives as CL86-C; an allowlist should describe the authorized fixture.

## Coverage gaps stated by the reviewer

- The original PDFs, EDS bytes, normalized delta and source manifests were not supplied, so
  page renderings, object types, defaults, hashes and section accounting were not checked.
- The Lichuan headers and implementation, ledgers, generators and checkers, owner dispatch,
  example routes and registrations were not supplied; existing call paths, legacy bypasses,
  ABI compatibility and resource cost remain unverified.
- The Q01 ledger row and the live connection report behind the CL86-C `1018:00 = 5`
  correction were not supplied; verify it against the raw specimen records.
- Current main PROGRESS, the DS2 identity stage and contract, the detailed shared intake
  inventory, the parent instructions and several main-sequence prompts were not supplied.
- A text audit: no checker, build, test, CI job or hardware procedure was run.

## Round 2

A condensed, faithful copy of the second report. It is untrusted reviewer input; the verified dispositions are
in [audit_application.md](audit_application.md) "Round 2". Line numbers refer to the files as revised by round 1
(committed in `bd2b30d9`; the family files' hashes in the report equal those of that commit). Prompt 57 and the
shared contract were audited before `a7656efb`.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-184724-01623c` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used, reasoning effort high, depth standard ("large input (~156316 tokens)") |
| Date | 2026-11-19 18:47:24 (from the ID), 204.9 s, one attempt |
| Revision | `main` at `edcf9621a1f5f598ca77d5e9261f885b7946f601`, no uncommitted changes recorded |
| Cost | Estimated USD 0.34 (0.343524, configured pricing); 122,057 input and 9,941 output tokens (4,110 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-184724-01623c.json` (local) |
| Location checks | 36 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_models_and_identity.md` (129, `30cd1c89`),
`02_model_ledger.md` (103, `50a2965f`), `03a_tlc_rows.md` (95, `e1df3d40`), `03b_tlc_operations.md` (131,
`c36a7fdf`), `04a_cl2_rows.md` (102, `ccea15a2`), `04b_cl2_operations.md` (113, `d065c550`),
`05_en57_conflicts.md` (64, `2256a5fb`), `06_console_and_coverage.md` (78, `dcc38879`), `07a_first_contact.md`
(78, `0cd8d217`), `07b_qualification_and_release.md` (69, `fd01ad4e`), `README.md` (136, `c6a488f8`), `audit.md`
(81, `3f1f2c11`), `audit_application.md` (81, `0125b9f8`), `execution_contract.md` (145, `c2df586f`). Context:
`canopen_shared/README.md` and `02_profile_integration_contract.md`, `ADD_DRIVE.md`, `COMMON.md`, the LX pack
`README.md` and `review.md`, `57_regular_drive_operations.md`, `docs/reference/lichuan_review.md`; project
instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The revision separates firmware reporting from model-specific behavioral evidence, keeps EN57
documentary-only and makes most previously circular first-contact questions experimental or NOT RUN. The
documentary ledger and fail-closed scaffolding can proceed, but the operation stages still conflict with prompt
57 on stops and leave transmit, completion and bench admission rules under-specified; several round-1 fixes hold
only in the example application or on the nominal path. **Verdict:** blocking issues. Do not execute the
device-facing stages as written: resolve the prompt 57 stop exception and identity precedence first, then make
the bus, stop and observation requirements enforceable at the stages that introduce those routes. Documentary
work and host-only scaffolding need not wait.

### Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The live identity guard suppresses stops that prompt 57 expressly exempts.* 01 withdraws a pending download and sends nothing further on a new CONTRADICTED outcome; 03b applies the refusal to every admitted TL-C operation and 04b inherits it, while prompt 57 keeps a reviewed, explicitly requested halt or quick stop admitted with the contradiction in its result. A stop prepared before the contradiction must transmit under one prompt and be withdrawn under the other. Consume the exception at preparation, at each changing step and at owner hand-off, limited to an otherwise-admitted reviewed stop that still passes route, generation and stop-specific checks; test a contradiction before preparation and after queueing (stop not refused for identity alone, prohibited writes retired, no standstill claimed). | 01:50-52; 03b:12-15; 57:102-104 |
| F2 | High, verified defect, high | *The inherited best-effort stop bypasses the new models' predecessor evidence.* Ordinary stops admit `000B` from predecessor 15 only, yet both stages keep the best-effort path on an unresolved route, before and after S3, on the same 15-to-11 example. S3 validates the transfer shape and binding generation, not the predecessor; after an uncertain write neither establishes the documented state. Separate ordinary stop admission from transmitting with an unknown predecessor; default-deny the latter for new models until a reviewed model decision covers it, coordinated with S3 rather than a parallel transmitter; test an unresolved route after a possibly sent controlword, a stale binding and unchanged legacy behavior. | 03b:43-46, 49-51; 04b:28-30; canopen_shared 02:224-227 |
| F3 | High, probable defect, high | *A known product mismatch can be reduced to a write-permitting UNCONFIRMED.* Prompt 57 refuses motion on a differing product identity but defines a no-match result as UNCONFIRMED unless another candidate matches; an observation that disagrees with the only reference of the declared model and matches nothing else is UNCONFIRMED, which the execution contract treats as enough for a vendor-object download. 03b's product-equality check covers only operations that take measured records. Resolve the precedence once in prompt 57 and consume it in both Lichuan sets; distinguish a mismatch, an unread field, a missing reference, a collision and a firmware-only difference; specify a reference's minimum product content; add exact-traffic tests for a mismatch with no alternate match, an incomplete observation and a firmware-only change. | 57:71-72, 87-89, 105-106; execution_contract:90-92 |
| F4 | High, verified defect, high | *Pre-S13 isolation covers schedule arming but not every interface-wide transmission.* 06 bars a TL-C/CL2 declaration while a schedule is armed and arming afterwards, but not an individual admitted SYNC aimed at the CL86-C node; per-model permission for broadcast NMT is not S13's live population-wide approval; the new-model API routes exist before 06's console restriction. Before S13 require isolation or deny individual and scheduled SYNC and mixed-interface node-0 NMT wherever a new-model binding is admitted, from the first usable binding, then consume S13's guard; test direct and scheduled requests targeting the legacy node, broadcast NMT and population changes with interface TX assertions. | 06:43-45, 49-51; canopen_shared 02:233-236; AGENTS.md:59 |
| F5 | Medium, probable defect, high | *External callers can start a new-model binding with unreviewed SDO reuse markers.* The example sets reuse to NONE, other applications get a Doxygen obligation; the channel default reads `1018`, which CL2 documents only through its explicit identity option and DEC-1 covers for neither group. A missed setup or a rebind restoring the default lets a timeout add unreviewed traffic; round 1 left the enforced boundary undecided. Make a verified reuse policy for the exact channel generation a prerequisite of new-model route admission (probe and identity included) or keep those routes blocked until an enforceable boundary is agreed, with owner checks outside the core and S10 coordinated; test missing and stale policy after rebind, timeout and late reply. | 03a:24-27; 04a:98-100; audit_application:51-53; canopen_shared 02:213-215 |
| F6 | High, probable defect, high | *Acknowledgement-only completion can lose the obligation to end a finite velocity run.* Without polling the operation ends with acknowledgement evidence and both groups use that branch; ending the lifecycle at the start acknowledgement, or waiting for the deadline before stopping, leaves no specified way to end the run, since the deadline forbids later steps and no drive host-loss reaction is documented. Keep the stop obligation when observations are skipped; name who holds the run or stop intent, when the stop is requested relative to the deadline, and behavior on start-acknowledgement loss, service delay and unavailable stop transport; refuse before start when the stop cannot be supported; report unknown motion when unconfirmed; require measurable start-to-stop criteria before 07b qualifies finite velocity. | 03b:28-32; 04b:17-18; canopen_shared 02:107-110; LX review:312 |
| F7 | High, verified defect, high | *CL2's mode check cannot show which mode produced a separately read statusword.* 04a notes `6041` and `6061` are not atomic but checks only acquisition membership, node generation and displayed against commanded mode; a statusword read in the old mode followed by a mode display in the commanded mode passes all three, and bit 10 enters 04b's completion with the wrong meaning. Require an evidenced mode-stability boundary over the whole acquisition, define what the owner serializes or detects and when completion stays UNKNOWN; test an interleaving with the returned mode equal to the command and the generation unchanged but the statusword sampled under another mode. | 04a:40-44; 04b:34-36 |
| F8 | High, probable defect, high | *First-contact production routes do not establish their own prerequisites.* T1 captures identity after 03a but stores it through 06's route, which consumes static records and defines no entry; T5 and K5's attended gate does not supply the source-state table, establish mode 1 or show the target equals the actual position in the command coordinate; mode select is unavailable before prompt 57 and may stay gated; a TLO position is reported, not measured. Separate identity capture from loading a reference and assign and test the loading path; for T5/K5 name an executable path to the initial state, mode, target coordinate and stop readiness, including an enable that stops partway, otherwise the reviewed experiment path or NOT RUN. | 07a:10-11, 42; 06:21-22; 03b:37-40; 04b:22-25 |
| F9 | High, verified defect, high | *First-power acceptance does not require resolving known electrical ambiguities.* 07a records wiring and picks the narrowest documented supply range, which cannot resolve the TL-C source labelling both power pins negative or the conflicting I/O wiring claims; 07b resolves fixture rating conflicts only after 07a has powered the device. Move the electrical gate before first power: a reviewed exact-fixture wiring and rating record resolving power connection, allowable supply and any connected I/O or brake ambiguity from applicable evidence; keep ambiguous optional I/O out until resolved; leave the bench step NOT RUN otherwise; infer no pin assignment from the conflicting text. | 07a:20-23; LX review:318-321, 452-455; 07b:36-37; AGENTS.md:83 |

### Unresolved questions

1. What stationarity evidence authorizes TL-C position clear while X04 is open, especially on TLO? 03b:73-76
   admits a reference-changing write from one report with alarm and running clear; the layout fixes decoding
   but not freshness or physical standstill, so the contract must separate a controller-reported predicate
   from a physical or caller-qualified precondition.
2. Who updates the admission map and API/CLI tests when prompt 57 lands after extension closeout? The stages
   describe both landing orders, but prompt 57's steps do not own the later update of the TL-C and CL2
   mode-select and release dispositions, so the map and console can keep stale NOT_IMPLEMENTED entries.

### Coverage gaps stated by the reviewer

- Original manuals, EDS files, rendered pages, the LX manifest and the normalized delta were not supplied:
  register metadata, ratings, translations, hashes and the three-EDS identity-default claim were not checked.
- Owner, service, profile, console, ledger, checker and build sources were not supplied: call paths, landed
  seams, configuration lifetimes and package registrations are unverified.
- Main PROGRESS, the shared intake inventory, the DS2 identity contract, parent instructions and the Q01/live
  records were not supplied: landing order, the sibling identity integration and the specimen-count
  correction are unverified.
- A text audit: no checker, compilation, test, CI job, port access or hardware procedure ran.

## Round 3

A condensed, faithful copy of the third report. It is untrusted reviewer input; the verified dispositions are in
[audit_application.md](audit_application.md) "Round 3". Line numbers refer to the files as revised by round 2
(committed in `83a7a232`; the family files' hashes in the report equal those of that commit, unchanged at the
audited `94138cd7`). Prompt 57 and the shared contract were audited before `c4e5574f` and `6ff80ed1`.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-191534-a7642a` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used, reasoning effort high, depth standard ("large input (~174816 tokens)") |
| Date | 2026-11-19 19:15:34 (from the ID), 191.4 s, one attempt |
| Revision | `main` at `94138cd7b88852de54261a2309b693f32636095e`, uncommitted changes recorded (outside this directory) |
| Cost | Estimated USD 0.36 (0.364016, configured pricing); 134,448 input and 9,512 output tokens (3,347 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-191534-a7642a.json` (local) |
| Location checks | 34 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_models_and_identity.md` (152, `b6d29eaa`),
`02_model_ledger.md` (108, `bb381e23`), `03a_tlc_rows.md` (114, `d62ea3d1`), `03b_tlc_operations.md` (158,
`e426997e`), `04a_cl2_rows.md` (110, `6c2e60e1`), `04b_cl2_operations.md` (121, `2ed5d929`),
`05_en57_conflicts.md` (64, `2256a5fb`), `06_console_and_coverage.md` (82, `f1bd4ba9`), `07a_first_contact.md`
(98, `f457d20d`), `07b_qualification_and_release.md` (75, `bfa59659`), `README.md` (142, `90298dda`), `audit.md`
(152, `9e2b09ae`), `audit_application.md` (165, `dfbd3de6`), `execution_contract.md` (151, `ccfb8db5`). Context:
`canopen_shared/README.md` (81 lines) and `02_profile_integration_contract.md` (329), `ADD_DRIVE.md`,
`COMMON.md`, the LX pack `README.md` and `review.md`, `57_regular_drive_operations.md`,
`docs/reference/lichuan_review.md`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The revision keeps the reviewed-stop exception to identity refusal, separates firmware reporting
from model-specific evidence and requires an electrical review before first power. The remaining blockers are
at transmit admission and motion recovery: the best-effort stop rules disagree about what the owner must
enforce, interface isolation does not cover commands already queued, and an admitted finite run can lose its
only stop route after a start timeout. **Verdict:** blocking issues. No consistent owner-level stop policy can
be derived from the stage, the shared contract and the lead disposition, and the finite-run and first-contact
enable stages lack an executable stop path for cases their own admission and failure rules permit; these are
specification and acceptance defects, not observed hardware behavior. The documentary ledger and fail-closed
scaffolding can proceed.

### Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The stage and lead disposition specify different best-effort stop boundaries.* 03b requires the owner to return `INVALID` with no TX when the application configures no shape for a TL-C binding, and 04b imports it for CL2, but the shared contract keeps DEC-2 for Lichuan bindings and the round-2 lead decision calls the family-local preparation refusal sufficient. A refusal in `prepareBestEffortStop` does not cover a direct `admitBestEffortStop`, and omitting a configuration is not specified to override the Lichuan default. Choose one boundary and align stage, contract and disposition: either specify and test an explicit per-binding disable that overrides the Lichuan default before first new-model traffic and after rebind, or drop the owner-level guarantee and name the owner entry points left outside the supported route. | 03b:59-60; 04b:31-32; canopen_shared 02:246; audit_application:159-160 |
| F2 | High, probable defect, high | *Pre-S13 isolation does not account for interface-wide commands already admitted.* The rule blocks new requests after a TL-C or CL2 binding appears and blocks declaration while a schedule is armed, but not a one-shot SYNC or node-0 NMT admitted earlier with no schedule armed; if admission and hand-off are separate, it can leave after the declaration. The tests cover only requests after declaration and an armed schedule. Define the serialization point for declaration, rebinding and interface-wide TX; refuse the population change while an affected command is outstanding, or retire unsent commands and recheck the population at hand-off; specify how handed-off traffic is reported; test SYNC and broadcast NMT admitted before declaration with dispatch held, then the population change. | 03a:79-81, 84-86; canopen_shared 02:277-279 |
| F3 | High, verified defect, high | *A stop-table match does not establish a usable stop route after a lost start reply.* A timeout under `NONE` leaves the route unresolved; the ordinary stop needs a fresh admitted predecessor and the unresolved-route stop is denied for both groups, so even full stop-table coverage can leave a possibly running drive with no callable stop. Keeping `needsStop` is correct reporting but supplies no stop. Make a usable stop path under the lost-start timeout a separate admission prerequisite; keep production finite velocity gated where no reviewed path meets the stop budget; test a possibly sent start and a timeout under `NONE`, refused before start when no path exists. | 03b:76-79; 04b:45-46; canopen_shared 02:234-236, 127-129 |
| F4 | High, verified defect, high | *First-contact enable asks for a prepared stop from a state the enable has not yet reached.* T5 and K5 require a stop from predecessor 15 prepared before enable, but ordinary stop admission needs a fresh observation of an admitted predecessor, and the enable does not start from predecessor 15. Planning and urgent-path readiness can precede enable; preparation cannot. The partial-enable text also does not separate a final word known unsent from one possibly sent with no usable reply. Separate planning and readiness from preparation against a fresh predecessor; define cutoff and reporting for interruption before word 15, a possibly sent word 15 with no observation and a confirmed predecessor; otherwise T5/K5 stay experimental or NOT RUN. | 07a:64, 79; 03b:52-53; 04b:28-30 |
| F5 | High, probable defect, high | *Position clear can change the reference on evidence the stage calls unqualified.* 03b admits a reference-changing write on a newly read controller report while stating that the bits' freshness is unqualified and TLO feedback is not measured; a fresh upload does not make the drive's state fresh. The result wording claims no standstill, but the admission predicate is unchanged. CL2 omits the mode-1 restriction of the C-family predicate, and neither stage excludes clearing while earlier motion is active or uncertain. Define admission separately: a current model-reviewed idle predicate, exclusion of unresolved motion, independent stationarity evidence where reports cannot establish it, explicit modes; keep the write gated otherwise; test stale zero or running-clear reports, an unresolved earlier move and a mode or generation change before dispatch. | 03b:92-94; 04b:66-68 |
| F6 | High, probable defect, high | *CL2 homing does not define a qualification and failure boundary for every admitted method.* 04b checks method numbers, wiring, direction and an X11 restriction for index methods but, unlike TL-C, does not require a method-specific behavioral record for every admitted method, and does not say who requests a reviewed interruption when a signal or completion transition never arrives, also without read acceptance. The general deadline stops operation steps, not physical homing, and no host-loss response is documented. Make X11's evidence explicit per model and method (validated signal sequence, bounded search, admitted interrupt predecessors); define the application's interruption obligation and what stays unknown if the stop or observation route fails; keep methods without it gated; test absent or stuck signals and observation loss or expiry after a possibly sent trigger. | 04b:55-57; 03b:81; LX review:259-262; ADD_DRIVE:25 |
| F7 | High, probable defect, high | *The CL2 peak-current setter does not specify enforcement of the motor limit.* 04b gives the setter a model drive range and leaves the motor rating as the caller's limit; X14 and 07b need fixture current bounds, but no input, comparison or missing-record refusal is specified, so an in-range value above the attached motor's limit can be prepared. The TL-C setter has an explicit rating gate, so the groups differ without a stated disposition. Define whether CL2 current writes are gated on a caller-supplied, fixture-scoped motor limit; require both bounds in the same current basis or keep the setter gated; test missing or invalid limits and a value inside the drive range above the motor limit, with zero TX. | 04b:76-77; 03b:100-101; 07b:39-40 |
| F8 | Medium, probable defect, medium | *The reuse attestation covers rebinds but not necessarily policy changes on the same channel.* The attestation records a readback of `NONE` against the channel epoch at preparation; the stage does not say whether the policy is immutable for that epoch, whether a change invalidates the attestation, or what happens to queued work and marker activity. If `setSdoReuse` changes policy without advancing the epoch, a stale attestation passes and a later timeout can add unreviewed marker traffic (distinct from the deferred raw-caller boundary). Pin `NONE` for the live binding on supported routes or make every policy mutation revoke the attestation and retire dependent unsent work; specify the serialization point; test a same-binding policy change around a queued request, then a timeout and a late reply: no marker or replay. | 01:58-60; 03a:24-25; audit_application:161 |
| F9 | Medium, verified defect, high | *The advertised independent CL2 path consumes TL-C stage artifacts.* The nominal order can satisfy the dependencies, but the README and 04a say CL2 rows proceed from 02 independently while 04a consumes 03a's isolation, attestation and tests and 04b consumes 03b's evidence and observation rules. If a TL-C stage is blocked, an implementer cannot tell whether to wait, land the common artifacts in 04a or duplicate them. Record the specific 03a and 03b artifacts as prerequisites, or move common admission and isolation into a named earlier deliverable, keeping CL2 documentary work independently executable. | README:42; 04a:19-21; 04b:13-14 |
| F10 | Medium, verified defect, high | *Physical qualification records timing observations without a pass threshold.* 07b calls the stop due time, urgent-stop TX time and first fresh report "criteria", but they are measurements: no latest acceptable stop TX, permitted stopping time or travel, or rule for what the post-stop report establishes, so two reviewers could judge one trace differently. Require an approved fixture- and operation-specific pass envelope before each qualification run; for finite velocity tie dispatch acceptance to the stop budget and state the independent observation and bounds behind any physical stopping claim; mark a run without criteria as characterization; use caller- or fixture-reviewed limits, not invented device timing. | 07b:47-51; AGENTS.md:71 |

### Unresolved questions

1. How does the single identity comparison classify a declared-model match when a competing reference is
   undecidable because one of its product fields was unread or wrong-width? Stage 01 caps an undecided
   reference at UNCONFIRMED, while prompt 57's CONFIRMED bullet says every competitor must have a reference
   and none match; admission may not change, but the reported confidence and any future CONFIRMED guard do.
2. Where is the executable state-read plan for T5/K5, and how does it establish that target and actual
   position are comparable for the exact fixture? The stages name the reads and reject writes that make the
   precondition hold, but equality of two raw values is not evidence of a shared coordinate basis.

### Coverage gaps stated by the reviewer

- Original PDFs, EDS files, manifests and the normalized delta were not supplied: register metadata, wording,
  ratings, hashes and the three-EDS identity-default claim were not checked.
- Lichuan headers and implementations, owner and service dispatch, example call sites, ledgers, checkers and
  CI workflows were not supplied.
- Main PROGRESS, the shared intake inventory, the DS2 identity stage and contract, parent instructions and the
  Q01 raw specimen records were not supplied.
- A text audit: no checker, compilation, test, CI job, port access or hardware procedure ran.

## Round 4

A condensed, faithful copy of the fourth report. It is untrusted reviewer input; the verified dispositions are
in [audit_application.md](audit_application.md) "Round 4". Line numbers refer to the files as revised by round
3 (committed in `a6b58cf1`; the family files' hashes in the report equal those of that commit). Prompt 57 and
the shared contract were audited before `328651b6`.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-193919-6d6362` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used, reasoning effort high, depth standard ("large input (~192448 tokens)") |
| Date | 2026-11-19 19:39:19 (from the ID), 176.8 s, one attempt |
| Revision | `main` at `a6b58cf12e7d71e24b7c95ddfce40bd5da173a0d`, uncommitted changes recorded (outside this directory) |
| Cost | Estimated USD 0.39 (0.38707, configured pricing); 146,115 input and 9,484 output tokens (5,185 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-193919-6d6362.json` (local) |
| Location checks | 25 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_models_and_identity.md` (161, `76d4414a`),
`02_model_ledger.md` (108, `ec4fdc84`), `03a_tlc_rows.md` (130, `7fc42f4b`), `03b_tlc_operations.md` (214,
`d657286c`), `04a_cl2_rows.md` (113, `ee89d7f1`), `04b_cl2_operations.md` (147, `d398e646`),
`05_en57_conflicts.md` (64, `2256a5fb`), `06_console_and_coverage.md` (85, `b77ef7c1`), `07a_first_contact.md`
(98, `ff171fb9`), `07b_qualification_and_release.md` (84, `37851cb0`), `README.md` (147, `ce72b794`), `audit.md`
(224, `9600ef06`), `audit_application.md` (250, `86674d30`), `execution_contract.md` (151, `ccfb8db5`).
Context: `canopen_shared/README.md` (81 lines) and `02_profile_integration_contract.md` (341), `ADD_DRIVE.md`,
`COMMON.md`, the LX pack `README.md` and `review.md`, `57_regular_drive_operations.md` (214),
`docs/reference/lichuan_review.md`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The revision keeps the reviewed-stop identity exemption, reports firmware differences without an
exact-firmware lock, and extends the pre-S13 rule to queued interface-wide traffic. Documentary and fail-closed
scaffolding can proceed, but the device-facing stages leave gaps in first-traffic stop protection, homing
interruption timing, first-contact enable admission and fault-clear transitions, and prompt 57's bootstrap
rules need explicit model-level dispositions. **Verdict:** significant issues. The gaps can be fixed without
changing the architecture or reversing the lead's caller-responsibility decisions; resolve them before enabling
the affected routes or running first-contact motion. Ledger, decoder and host-only refusal work can continue.

### Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *The explicit best-effort stop denial can land after a new model's first traffic.* 03a permits TL-C reads before S3's explicit override exists and 04a inherits that setup; if S3 has not landed, the override waits for an operations stage, contradicting the shared rule to set the no shape before first traffic. A timed-out upload in a read-only stage can leave the route unresolved, and the existing service accepts the legacy shape on every channel; a profile-level refusal does not cover a direct owner call. Make the separable override a checked prerequisite of the first new-model traffic; the reads stage that runs first consumes or lands it before either group has traffic. Test the reads-stage checkout: after a timed-out identity upload, direct best-effort admission returns `INVALID` with zero TX, also after rebind. | 03a:32-33; 04a:10-11; 03b:72-73; canopen_shared 02:255-257; canopen_shared README:54 |
| F2 | High, probable defect, high | *Homing has a search bound but no explicit stop budget or transfer cutoff at that bound.* The shared timed-run rule reserves a stop budget, caps transfers at stop due and gives the urgent stop its own deadline; homing imports only the stop-path prerequisite and a bound (travel or time) ending before the operation deadline. An observation SDO can stay in flight past the bound, a travel overrun can be seen only at the next sample, and no time is left for the ordinary HALT; a declared independent stop does not say when to use it. Specify a caller-supplied latest interruption time and stop budget even with a travel bound; define the evidence and maximum observation age for a travel bound; cap transfers at the stop cutoff; handle the separate HALT and the declared fallback when service resumes late or the channel is unresolved. Test an upload in flight at the bound and a delayed service cycle; keep `needsStop` and unknown motion/reference without fresh evidence. | 03b:108-110; 04b:71-72; canopen_shared 02:123-125; audit_application:239-240 |
| F3 | High, verified defect, high | *First-contact enable substitutes raw equality for its target-at-actual prerequisite.* 03b and 04b require the attended enable to start in mode 1 with the target equal to the actual position; T5 and K5 accept raw `607A` equal to `6064` as a screen while leaving their basis unestablished. The same integer can mean different positions, and on TLO the report is not measured feedback; rereading the target and attending a cutoff do not establish the prerequisite. Require fixture-specific evidence that both values are comparable before the production route; otherwise keep the comparison as a screen and use a separately assigned experiment that explicitly allows and bounds unexpected motion, or NOT RUN. Test that raw equality without the basis does not admit the production route. | 03b:44-46; 04b:26-27; 07a:64, 79; AGENTS.md:60 |
| F4 | High, verified defect, high | *Neither alarm-clear stage defines the required source-state table.* The shared contract requires a source-state and intent table before fault-clear TX and a first-frame-or-none test per row. TL-C names a method and cites a controlword example; CL2 lists two methods and cautions that reset behavior varies by fault. Neither says which fresh states admit each method, what an already-clear request does or which states refuse; choosing a method is not an evidenced transition, and T9's naturally occurring alarm is not a sufficient production prerequisite. Define and test per-model, per-method reset transitions with fresh fault and mode evidence, already-clear behavior and uncertain assertion or deassertion; keep rows without evidence gated; make T9 name the admitted row it consumes. | 03b:116; 04b:76-77; canopen_shared 02:139-140; 07a:68 |
| F5 | Medium, probable defect, high | *Prompt 57's bootstrap exceptions are not reflected in the model admission rules.* Prompt 57 separates bootstrap identity and state reads from ordinary work on a contradicted binding and admits a targeted Enter Pre-operational from a fresh Stopped report under every outcome but CONTRADICTED. Stage 01 guards changing work with only the stop exemption, and 03a exposes configuration and PDO-map uploads without saying how contradiction restricts them. The family may withhold the NMT command, but the text neither records that nor distinguishes it from an omission. Add a model-level admission table for bootstrap reads, ordinary reads, targeted NMT and broadcast NMT, tested at the application route and owner hand-off where applicable; name the reads that remain on contradiction; for targeted Enter Pre-operational either name the reviewed fresh Stopped observation that admits it or record why the model withholds it and report the stranded node without an automatic fallback. | prompt 57:99-101, 119-121; 01:156-157; 03a:77-78 |
| F6 | Medium, verified defect, high | *The CL2-first schedule omits two shared safety prerequisites from its ownership rule.* The README allows 04b to run first and lists the common artifacts it must land, but omits 03b items 8 and 10, while 04b imports item 8's retained last-motion terminal and pre-write rechecks and item 10's current-limit record. Add their shared parts, with tests, to the first-run ownership rule in both stages and the dependency table, or make them explicit prerequisites of only the affected 04b deliverables. | 04b:9-11, 83-84, 95-97 |

### Unresolved question

1. Which outcome does the one comparison return when a competing model definitely matches but the declared
   model's reference is undecided because a required field was unread? Stage 01 gives UNCONFIRMED without a
   declared match for an undecided declared reference with no other declared match, while prompt 57 gives
   CONTRADICTED for a competing match with no declared match and lists unread fields under UNCONFIRMED. Both
   refuse normal changes, but the diagnostics differ, and so can the targeted Stopped-node bootstrap; the
   comparison needs one precedence and a regression for this combination.

### Coverage gaps stated by the reviewer

- LX originals, rendered pages, EDS bytes, the normalized delta and manifests were not supplied: object
  metadata, translations, electrical limits, hashes and the CL86-C specimen-count correction were not checked.
- The repository index was truncated; owner, service, profile, example, ledger, generator, checker, workflow
  and test sources were not supplied, so call paths, guard lifetimes, package registration and resource effects
  are unverified.
- Main PROGRESS, the shared intake inventory, the DS2 identity stage and the parent instructions were not
  supplied.
- A text audit: no checker, build, test, CI job, port access or hardware procedure ran.

## Round 5

A condensed, faithful copy of the fifth report. It is untrusted reviewer input; the verified dispositions are
in [audit_application.md](audit_application.md) "Round 5". Line numbers refer to the files as revised by round
4 (committed in `b661fba7`, unchanged at `570320bb`; the family files' hashes in the report equal those of that
revision). The shared contract was audited before `2d1c70c2`.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-201350-68dc0f` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-sol` requested and used, reasoning effort high, depth standard ("large input (~207888 tokens)") |
| Date | 2026-11-19 20:13:50 (from the ID), 209.8 s, one attempt |
| Revision | `main` at `570320bb4bd9db9cb1035d942b5b98177614cd8b`, uncommitted changes recorded (outside this directory) |
| Cost | Estimated USD 0.43 (0.427022, configured pricing); 156,846 input and 11,333 output tokens (6,645 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-201350-68dc0f.json` (local) |
| Location checks | 26 verified, 0 relocated, unverified or invalid |

Promptset under audit (lines, sha256 prefix): `01_models_and_identity.md` (167, `7c36b431`),
`02_model_ledger.md` (108, `028b4bf6`), `03a_tlc_rows.md` (149, `50193cc1`), `03b_tlc_operations.md` (252,
`7dd730ca`), `04a_cl2_rows.md` (118, `2192fed2`), `04b_cl2_operations.md` (172, `b9ff0d03`),
`05_en57_conflicts.md` (64, `2256a5fb`), `06_console_and_coverage.md` (85, `4697a61f`), `07a_first_contact.md`
(100, `4a541425`), `07b_qualification_and_release.md` (85, `14d73f79`), `README.md` (150, `6696145c`), `audit.md`
(290, `5566cb7a`), `audit_application.md` (331, `71662bc8`), `execution_contract.md` (177, `b7f63921`).
Context: `canopen_shared/README.md` (81 lines) and `02_profile_integration_contract.md` (356), `ADD_DRIVE.md`,
`COMMON.md`, the LX pack `README.md` and `review.md`, `57_regular_drive_operations.md` (217),
`docs/reference/lichuan_review.md`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** Documentary and fail-closed scaffolding can proceed, but the device-facing stages need corrections
first. The reviewed-stop identity exemption and the best-effort denial before first traffic are present. The
remaining issues: when the application must use the independent stop, what attended enable proves on a TLO
drive, and what happens to queued traffic and owner settings when a model declaration changes. **Verdict:**
significant issues. The single-profile architecture and the lead's caller-responsibility decisions need not
change; the affected motion and declaration paths need explicit failure behavior and traffic-level tests
before use. The prompt 57 sequencing gap is repairable without delaying ledger, decoder and host-only work.

### Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, probable defect, high | *The independent stop is named, but the application has no explicit deadline for using it.* The velocity rule covers an urgent stop that cannot be admitted, and homing keeps the declared stop when HALT cannot be admitted, but neither says what the application does when the urgent stop is admitted and misses its dispatch window, loses its acknowledgement or leaves motion unresolved. Homing can interrupt before the latest interruption time while an observation upload is pending; capping that upload at the latest time does not free the route at the earlier interruption. An implementation could pass the stated checks while leaving the drive running and only reporting `needsStop`. Keep actuation application-owned; define when the application must invoke the declared stop (an early homing interruption with an upload pending, a missed urgent-stop deadline, an effect unconfirmed at the fallback deadline); admission, TX or acknowledgement are no evidence that the fallback is unneeded; add service-level tests that the fallback is requested by the specified time, keeping `needsStop` and uncertainty. | 03b:102-105, 119-122, 123-126; 04b:76-81; canopen_shared 02:119-143; AGENTS.md:75 |
| F2 | High, verified defect, high | *The attended TLO enable still cannot establish its target-at-actual-position prerequisite.* The basis record makes `607A` and `6064` comparable but does not make `6064` the shaft's physical position on an encoderless TLO; the stages acknowledge `6064` is a controller report yet let the production attended route meet the prerequisite with it. Rereading the target does not close the gap. Require fixture evidence of the actual position at admission, or treat the equality as a controller-coordinate screen and use the separately reviewed experiment that bounds unexpected motion; add a negative test with equal raw values and a valid basis record but no actual-position evidence; keep the cutoff. | 03b:46-51, 51-53; 07a:66; LX review:300-301 |
| F3 | High, probable defect, high | *The first-traffic guard does not cover an already-queued targeted NMT command.* The read stages refuse new targeted NMT before owner admission, but the pre-S13 population-change check names only pending SYNC and node-0 NMT. A targeted command admitted while the address was legacy can reach hand-off after it becomes a TL-C or CL2 binding; stage 06's refusal while work is admitted is not required and tested for declaration and rebinding at the first-traffic stage, and round 4's "no hand-off case remains" does not follow. Apply the serialized check to pending targeted NMT for the affected address: refuse until terminal, or retire it and account for a possibly sent frame; held-hand-off tests for declaration and rebinding, unrelated targeted NMT untouched; consume the shared hand-off guard where landed. | 03a:90-97, 103-107; 06:18-21; canopen_shared 02:300-303 |
| F4 | High, probable defect, high | *TL-C homing checks configured input assignments but not the current fixture's usable signals.* 03b reads the DI assignments, which do not show that the method's home or limit switch is wired and usable here; 07a needs actual switches for its experiment, but the callable TL-C operation lacks the CL2 prerequisite pattern, so a record from one installation could admit a search on another with equal assignment bytes and a missing signal. The interruption deadline bounds the wait, not the signal. Per method, require current fixture evidence for the signals and starting conditions it needs beside the behavioral record; refuse before setup or trigger if a signal is missing, assigned differently or backed only by stale fixture evidence; test correct assignment bytes with a missing physical signal declaration. | 03b:106-110; 04b:66-69; 07a:69; AGENTS.md:85 |
| F5 | Medium, probable defect, high | *Re-declaring a legacy model has no defined transition out of the new-model owner settings.* Stage 06 sets `NONE` and the explicit no shape for a TL-C or CL2 declaration but gives no inverse when an idle node is re-declared CL86-C or OL86-C; the explicit no shape survives rebind, so the legacy declaration can look valid while its DEC-2 best-effort stop stays disabled. Blindly restoring legacy settings is wrong too while the previous channel has unresolved work. Require a fresh validated legacy binding, or restore the reviewed legacy settings only after route and work are reconciled; test both transitions on a reused channel, an unresolved previous transfer included, with the effective shape and reuse policy. | 06:18-22; canopen_shared 02:267-270; 03b:82-85 |
| F6 | Medium, verified defect, high | *Prompt 57's handoff omits the interval between the model ledger and extension closeout.* After stage 02 the checker rejects a NOT_IMPLEMENTED cell once its function is in the map, but prompt 57 assigns the update only after closeout; a prompt 57 landing between stage 02 and 03b, 04b or 06 meets the checker while the console tests and coverage it should update may not exist. Make prompt 57 update the cells whenever the stage-02 map exists, with the gated disposition where prerequisites are missing; update console tests and coverage only where they exist, otherwise name the owning stage; add a checker or handoff case for that order. | 02:86-90; prompt 57:201-203; 06:59-64 |
| F7 | Medium, verified defect, high | *The shared read-route table does not distinguish TL-C-only uploads from CL2 reads.* The table admits ordinary reads, raw bitrate candidates included, for both groups, and 03a tests ordinary request frames on an admitting outcome, while 04a imports those tests and forbids undocumented CL2 reads except `1018`; `2402` and `2407` are undocumented on CL2. Make the table an identity filter after model-and-object eligibility, or split common and model-specific rows; test the TL-C candidate read emitting its uploads and the equivalent CL2 request emitting none under the same outcome; keep the CL2 identity uploads a separate exception. | execution contract:109; 03a:95-97; 04a:81-82, 113-116 |

### Unresolved question

1. What are the evidence-backed starting rows for the first TL-C and CL2 attended enable, including the outcome
   when word 15 is acknowledged but the fresh report shows no admitted stop predecessor? The stages require a
   source-state table without its initial rows or the handling of an already-enabled or ambiguous report; T5
   and K5 cover a missing or unacknowledged report and a confirmed predecessor 15, not an acknowledged write
   followed by a fresh, different report. A bench handoff needs the first frame or no TX and the cutoff branch,
   or the case stays experimental or NOT RUN.

### Coverage gaps stated by the reviewer

- LX originals, EDS bytes, rendered pages, manifests, the normalized delta and Q01 raw specimen evidence were not
  supplied: vendor encodings, electrical limits, hashes and live identities were not independently verified.
- The repository index was truncated; owner, service, profile, example, ledger, generator, checker, test and
  workflow sources were not supplied, so queue retirement, binding-setting lifetimes, signatures and build
  integration are unverified.
- Main PROGRESS, the shared intake inventory, the DS2 identity stage, the parent instructions and prompts 64,
  65, 72 and 77 were not supplied.
- A text audit: no checks, builds, CI jobs, port activity or hardware tests ran.
