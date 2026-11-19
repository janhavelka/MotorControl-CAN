# Independent audit of the shared profile promptset, 19 November 2026

A condensed, faithful copy of the reviewer's report. It is untrusted reviewer input; the
verified dispositions are in [audit_application.md](audit_application.md). Line numbers
refer to the files at the audited revision, before the revision that applied the findings.

| Field | Value |
| --- | --- |
| Audit ID | `aud-20261119-171059-b00ba0` (tool `audit_promptset`, server 1.1.0, prompt version 2026-11-18.2) |
| Model | `gpt-6.2-astra` requested and used, reasoning effort xhigh, depth complex |
| Date | 2026-11-19 17:10:59 (from the ID), 259.8 s, one attempt |
| Revision | `main` at `f3c3870a66d2adea896a941fc786ac85204d07ae`, no uncommitted changes recorded |
| Cost | Estimated USD 1.37 (1.370456, configured pricing); 108,512 input and 12,559 output tokens (5,689 reasoning) |
| Report | `C:\Users\Honza\.openai-auditor\reports\202611\aud-20261119-171059-b00ba0.json` (local) |
| Location checks | 42 verified, 0 relocated, unverified or invalid |

Promptset under audit: `README.md` (56 lines, sha256 `14551e2b...`),
`01_existing_code_and_tooling.md` (122, `44e00a08...`), `02_profile_integration_contract.md`
(177, `e50f59f6...`). Requirements: `ADD_DRIVE.md`, `multidrive_decision.md`, `COMMON.md`,
`CONVENTIONS.md`, `MULTIDRIVE.md`. Context: `docs/prompts/README.md`, `PROGRESS.md`,
`docs/reference/README.md`, `Discovery.h`, `profiles/lichuan_c/State.h` and `Actions.h`,
`scripts/check_promptset.py`; project instructions `AGENTS.md`, `CLAUDE.md`.

**Summary.** The contract separates family behavior from common motion, confines the bench
waiver to its fixture and requires evidence before hardware claims, but is not yet usable as
a universal integration contract: a non-CANopen family cannot satisfy its acceptance tests
without violating its architecture rules. For CANopen families the risks are an undefined
mapping from application profile numbers to families, weak tests of live device
applicability and Lichuan-specific behavior on unresolved SDO routes. **Verdict:**
blocking issues; the needed changes are to the work orders and acceptance criteria, not
evidence of a malfunction in the existing drive.

## Findings

| ID | Severity, class, confidence | Finding and recommendation | Locations |
| --- | --- | --- | --- |
| F1 | High, verified defect, high | *A non-CANopen family must both avoid and use SdoClient to pass.* Stage 01 forbids SdoClient for a proprietary protocol; stage 02 requires every route through it, prescribes EDS/PDO ledger fields and a Lichuan axis plus a family axis on one interface, assuming coexistence instead of the isolated default. Split stage 02 into common, CANopen-only and other-protocol rules; other protocols use their native ledger and service through the real owner, test their real ack/no-ack and refusal of incompatible routes; prove multi-profile use on separate interfaces by default and on a shared one only after prompt 63's routing and compatibility artifacts pass; an unavailable native service or probe is Blocked, not routed through SDO. | 01:97-99; 02:32-34, 125-127, 172-173; AGENTS.md:59 |
| F2 | High, probable defect, high | *S2 requires a profile relation but defines neither key nor migration.* `Endpoint.profile` is application-assigned; `LICHUAN_C` is the implicit value zero; Lichuan preparations accept any nonzero value. An enum cast, a reserved numeric namespace and an application table behave differently for existing callers, and two-direction tests with new values would not detect it. Choose one representation before any guard; say where the number resolves to a family and protocol, the relation's lifetime, unknown or conflicting mappings and migration of Lichuan bindings; use it for preparation, owner admission and console; test existing labels, unknown values, protocol mismatch and rebinding with a retained result; never default to Lichuan. | Discovery.h:14-16; README:35; 02:77-79 |
| F3 | High, probable defect, high | *Foreign-label tests do not prove a correctly labelled binding points to an applicable drive.* The refusal test changes the binding's software profile; it does not cover a family-A label on a B server, a generic response, or a reboot since A's identity was read. `DiscoveryCapabilities` grants no action. Per device-facing operation, name the product/interface and firmware/configuration evidence consumed, its holder and invalidating lifecycle events; separate a permitted bootstrap read from permission for later writes; test the correct label with wrong, missing, ambiguous and stale identity; assert exact traffic (selection and cached queries send nothing; probe and recovery append no enable, save, NMT or other mutation). | 02:73-79; Discovery.h:28-29; AGENTS.md:75, 77 |
| F4 | High, probable defect, high | *Review before `setSdoReuse` does not protect a family that uses the default.* The default markers 1000:00 and 1018:01-03 rest on Lichuan DEC-1 and the default reuses with a marker; omitting the setter can issue an unreviewed upload after a timeout, which an ordinary probe never exercises. Require a resolved policy before a family route is usable (else reuse disabled or route unresolved); check the policy's granularity; inject timeout and late replies on a two-family owner, including no explicit configuration, and assert marker frames, correlation marking and no replay. | 01:48; 02:99-100; PROGRESS.md:69 |
| F5 | High, probable defect, high | *S3 specifies a stop frame shape, not the authorization and result contract.* The Lichuan best-effort path yields one allowed write, never succeeds and cannot be confirmed by a matching-looking reply; stage 02 does not bind the shape to the current target or say what happens without an evidenced stop. Make S3 opt-in per binding; at admission check the current target/epoch and its reviewed exact shape; no evidence refuses with no TX and no fallback; specify retirement of obsolete work and retained uncertainty; test direct owner admission with the other family's shape, no shape, a stale target and a matching late reply; preserve Lichuan semantics. | 02:97-98, 164-165; Actions.h:432-435, 782-784 |
| F6 | High, probable defect, high | *The header-checker split is scheduled after the first headers that may need it.* Stage 02 binds from registration, after ledger and codec, but the Lichuan checker scans every public header and stage 02 owns the split, so an earlier codec header forces an unassigned change, a misclassification or a failing checkpoint (dependency inferred, no failure observed). Move source/ledger rules to intake; split S7 before or with the first new public header, with an owner and tests that the header sets are disjoint and complete and unclassified common and Lichuan functions still fail. | 02:4-6, 49-51; 01:58; AGENTS.md:25 |
| F7 | High, verified defect, high | *The seam table records the implementer afterwards and blocks consumers by assignment.* The README blocks dependent work when the assignment lacks the owning prompt even if the artifact has landed and passed, contrary to COMMON's usable-prerequisite rule; recording after implementation lets two executors start the same seam. Separate authority to implement from permission to consume; record the claiming lead and stage before shared edits, then the landed commit, tested interface and consumer gate; consume a passed artifact without re-executing its prompt; if missing, block only the dependent deliverable; say which tests run before S4/S5; recheck the table before a seam. | README:22-24, 28-30; COMMON.md:89; ADD_DRIVE.md:5 |
| F8 | Medium, verified defect, high | *The JSONL profile field is undefined for records without an admitted operation* (help, profile list, interface statistics, parse errors, pre-admission refusals) and for a retained result after rebinding. Define a record-by-record rule, a profile-neutral value without a target, validation against the correlated request or operation; test two families interleaved, a retained result after rebind, untargeted help, an invalid request and a mismatched response; document compatibility with the existing runner. | 01:88-89; 02:110-112; CONVENTIONS.md:344-345 |
| F9 | Medium, verified defect, high | *The manifest instruction is stale.* The checker globs every pack's `SOURCES.json`; appending to `reference_manifests` is unnecessary and could check a pack twice. The README understates the checker as links and tables only; it also verifies artifact hashes. Place the manifest in the discovered location and run the working-tree, INDEX and HEAD checks; test required presence separately if deletion must fail; correct both descriptions without implying compilation or qualification. | 01:59; 02:61-62; check_promptset.py:75-77, 104-105; README:55-56 |
| F10 | Low, verified defect, high | *The StopKind inventory omits the mode-6 HALT exception.* HALT is 010F in modes 1/3 but a derived 000F from acknowledged 001F in mode 6. Describe StopKind as mode- and predecessor-specific, point to its declaration and keep it apart from the best-effort stop. | 01:69; Actions.h:87-89 |
| F11 | Medium, probable defect, medium | *Unclear whether every later family stage inherits the full registration CI gate.* "Binding on every family stage" plus the final acceptance would require firmware builds and CI for documentation-only stages, against the impact-based policy. Distinguish standing rules, one-time registration acceptance and per-stage verification; full matrix and exact-commit CI for registration and major changes, affected local checks otherwise. | 02:4-5, 175-177; AGENTS.md:7-8 |

## Unresolved questions (reviewer)

1. Is SDO reuse policy selected per owner, channel or binding, and when can a default marker
   first be emitted? (Decides whether a two-family owner can hold different reviewed marker
   sets.)
2. Which tested application artifact, if any, can run a family-specific read-only console
   probe before prompt 64 without a second session? (Decides whether registration can close
   before S4.)
3. Does the package export include new profile directories by pattern, or must each family
   edit the package manifest as well as the package assertions?

## Coverage gaps (reviewer)

Not supplied to the reviewer: `Endpoint.h`, `src/Discovery.cpp`, preparation
implementations, the owner/SDO implementation (including `validBestEffortStop`), console and
runner code; the full texts of prompts 57, 63, 64 and 65; the ledger and operation checkers,
generator, package tests, verifier and workflows; any family's numbered stages, live sibling
checkouts or new fixture evidence. The audit was text-only: no checker, build, test, CI or
hardware procedure ran.
