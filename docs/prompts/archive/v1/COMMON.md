> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Mandatory contract for every stage

Every numbered prompt incorporates this file. Execute its recurring audit and report requirements even when a stage is resumed in a new session. These are implementation instructions, not evidence that the work already exists.

Apply the selected contracts in [CONVENTIONS.md](CONVENTIONS.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md). CONVENTIONS is the single naming/status/health decision source; numbered stages make its implementation concrete. If new source evidence requires a change, update that contract and affected prompts/docs/tests together. Do not quietly introduce a competing convention.

## First actions

1. Read root and applicable parent `AGENTS.md`, this file, the numbered prompt, its predecessor's report, `docs/requirements_matrix.md`, `docs/architecture_plan.md`, `docs/api_cli_mapping.md`, and `docs/multidrive_decision.md`. Apply [MULTIDRIVE.md](MULTIDRIVE.md) for each stage's stepper/servo/protocol acceptance criteria.
2. Apply root AGENTS Git workflow: inspect/fetch branch/upstream and preserve concurrent work. At the start of a new promptset execution, create and record one new dedicated branch from the agreed base; for every later prompt or resumed session, use that same recorded branch. Inspect CI. Inspect live source in MotorControl-RS plus ADS1115 and INA228, then the I2C/reference repositories named by the stage. Read their relevant scoped agent instructions as context; CAN Git authorization comes from this user's instructions/root AGENTS, while sibling bench permissions do not transfer.
3. Record current sibling HEADs and dirty status. Read the actual relevant headers, implementations, callers, CLI, native tests, Python tests/runners and workflows. Inspect FieldCore's relevant ownership seam read-only. Use the audit reports and copied baseline to locate code, then verify current behavior. Audit the affected seam and revision delta; do not reread unrelated modules or repeat unchanged suites for each completed checkpoint. If a sibling is unavailable, state it and use the pinned snapshot for the covered seam; do not pretend a live audit happened or block unrelated work.
4. Verify source identity, original PDF/EDS hashes and model/firmware applicability for any device-dependent change. Read exact PDF pages where tables or diagrams matter. Resolve contradictory documents explicitly; no inferred constants, fallback defaults or invented firmware identity.
5. Produce a short implementation plan within the stage report, then implement the bounded stage. Continue without routine confirmation. Ask only for information or authorization actually needed by a dependent operation, while continuing independent work.

## Cross-cutting rules

- Match RS motion semantics and direct naming; preserve RS `Status`, `Err`, free `Ok()`, `Status::isOk()` and free `errToString()` style. I2C helpers differ. Keep actual owner `admit/admitUrgent/service/result/release/cancel` names where their meanings apply. Do not mechanically reproduce RTU errors or force I2C bind/poll lifecycles into a stateless motion core.
- Use fixed storage, bounded work per call, explicit capacities and value semantics. Validate complete candidates before publishing contexts or scheduling writes. Use explicit unsupported results with actionable reason and capability evidence; never success stubs.
- Keep one physical CAN owner. Within a CANopen domain, one authoritative SDO/PDO stack serves the selected vendor profiles; other protocols use their own service backend without another physical receiver. Decide ownership before building machinery. Do not instantiate a second stack in a console, profile, runner or observer.
- Handle unsolicited events independently of an active request; preserve node/channel/binding/configuration generations. CANopen has no wire transaction ID for ordinary SDO transfers. A local token cannot prove a late response belongs to a new identical request; the owner must prevent ambiguous channel reuse and report unresolved cases.
- Preserve raw SDO abort (all 32 bits), CAN controller evidence, statusword/controlword and EMCY data. Distinguish local validation failure, transport failure, valid protocol rejection, driver fault and operation uncertainty.
- Never retry uncertain side effects automatically. Document controller link-layer retransmission versus application replay and ensure queued pre-recovery frames cannot execute later as fresh intent.
- Health/status/result inspection is cached; explicit reads refresh. `stats reset` affects counters only. `recover` affects host transport only. `cancel` does not stop the drive. Motion and persistent/network changes remain explicit.
- Keep common functions usable from native C++, Arduino, ESP-IDF, FieldCore-like owners and other frameworks. No mandatory FieldCore, Python, CANopen stack or SDK dependency in the exported core.
- Keep the full documented native family surface in a ledger and expose typed operations, not just a raw dictionary escape hatch. Unsupported firmware/unknown behavior is recorded as an unresolved feature, not silently counted as complete.
- Support stepper and servo intent through capabilities rather than motor-type branching. Keep common axis identity/units/evidence independent of CANopen and native RPM/count widths; motor/feedback/mode are separate facts. Validate N5 stepper+BLDC and EPOS4 contrasts before freezing the API, and a non-CANopen routing/representation fixture before release. These are test-only proofs, not installed placeholder drivers or hardware support claims.
- Preserve copied MIT attribution. Vendor/standards material is reference-only and excluded from software license and distributable core bundles.
- Update affected durable documentation, CHANGELOG, examples and in-source Doxygen alongside behavior. Use precise units, lifetimes, failure effects and source evidence. Run the documented API-doc checks for affected declarations; stage 19 verifies their completeness rather than writing them for the first time.
- Prefer direct functions, clear names and explicit state transitions. Each new module or abstraction needs a current caller and a reason simpler code cannot express the requirement. Keep common contracts in one maintained location. Use plain engineering language and avoid speculative layers, duplicated engines, boilerplate comments and arbitrary size/coverage quotas.

## Verification and report

Commit and sync each coherent, verified completed chunk under root AGENTS, including useful checkpoints during a stage and all completed changes before stage handoff. Keep the entire promptset on one branch. The lead stages only task-owned files/hunks, reviews the staged diff, pushes to the established upstream and verifies synchronization. No routine confirmation is needed. A no-op stage needs no empty commit; a sync failure is reported accurately and never solved with force-push or discarded concurrent work. Frequent pushes do not require a manual full CI run for every small change; apply the parent/root CI policy by impact.

Run the stage's focused checks and affected CI commands. Add meaningful positive and negative tests for new behavior, concurrency/late-event handling, capacity and uncertainty. Never claim a fake node test proves real drive behavior. Repeat/broaden checks only for changed code or unresolved concerns.

Write `docs/reports/stage_NN.md` containing:

```text
Stage / status / date / implementation revision or working-tree identifier
Inputs: sibling revisions + dirty states + actual files inspected
Source evidence: document IDs/pages + EDS sections + conflicts resolved/open
Decisions: naming/ownership parity and deliberate differences
Changes: public API, behavior, ownership, capacity, CLI/protocol impact
Documentation: changed guides/comments/examples/CHANGELOG; justified N/A items
Maintainability: reused/removed code; reasons for new types/modules; simpler choices
Delegation: task/model/effort/file ownership and integrated review, or solo work
Checks: exact commands + results + artifacts; separate skipped/blocked checks
Git: promptset identity, execution branch/base/upstream, checkpoint commits, sync result/blocker
Coverage: documented / implemented / host tested / hardware qualified
Open issues: consequence, dependent stage, precise evidence needed
Handoff: next executable stage and any feature-specific gates
```

Update `docs/prompts/PROGRESS.md` and the stage report. Update `docs/backlog.md`, `docs/roadmap.md` and capability/object/API/CLI coverage when their respective facts change; link shared contracts instead of copying the report into each document. If a stage is partially blocked, finish independent items and keep it partial. Do not call the whole library complete because the prompts ran, a build passed, or a feature returns UNSUPPORTED. Do not require hardware to complete host-only stages.

For multi-agent implementation, the user permits model selection according to task complexity. Follow DELEGATION: inspect actual available options, delegate independent read-only reviews or disjoint files on the same promptset branch, declare ownership, and let the lead integrate shared contracts/build registration/Git changes. Use independent strong review for ambiguous protocol/stop/ownership work. Run final checks against the integrated result before committing/syncing it. Avoid concurrent edits to generated catalogues or central API headers; continue with an available model if overrides are unsupported.
