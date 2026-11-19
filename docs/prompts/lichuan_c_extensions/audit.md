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
