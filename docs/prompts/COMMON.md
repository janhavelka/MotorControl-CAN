# Mandatory contract for every stage

Every numbered prompt incorporates this file. Execute its recurring audit and report requirements even when a stage is resumed in a new session. These are implementation instructions, not evidence that the work already exists.

## First actions

1. Read root and applicable parent `AGENTS.md`, this file, the numbered prompt, its predecessor's report, `docs/requirements_matrix.md`, `docs/architecture_plan.md` and `docs/api_cli_mapping.md`.
2. Inspect current working tree and CI. Preserve unrelated files. Inspect live source in MotorControl-RS plus ADS1115 and INA228, then the I2C/reference repositories named by the stage. Read their relevant scoped agent instructions as context; sibling bench permissions and Git policies do not authorize actions in this checkout.
3. Record current sibling HEADs and dirty status. Read the actual relevant headers, implementations, callers, CLI, native tests, Python tests/runners and workflows. Inspect FieldCore's relevant ownership seam read-only. Use the audit reports and copied baseline to locate code, then verify current behavior. If a sibling is unavailable, state it and use the pinned snapshot for the covered seam; do not pretend a live audit happened or block unrelated work.
4. Verify source identity, original PDF/EDS hashes and model/firmware applicability for any device-dependent change. Read exact PDF pages where tables or diagrams matter. Resolve contradictory documents explicitly; no inferred constants, fallback defaults or invented firmware identity.
5. Produce a short implementation plan within the stage report, then implement the bounded stage. Continue without routine confirmation. Ask only for information or authorization actually needed by a dependent operation, while continuing independent work.

## Cross-cutting rules

- Match RS motion semantics and direct naming; preserve sibling `Status`, `Err`, `Ok()`, `isOk()`, `errToString()` style. Do not mechanically reproduce RTU errors or force I2C bind/poll lifecycles into a stateless motion core.
- Use fixed storage, bounded work per call, explicit capacities and value semantics. Validate complete candidates before publishing contexts or scheduling writes. Use explicit unsupported results with actionable reason and capability evidence; never success stubs.
- Keep one physical CAN owner and one authoritative SDO/PDO stack per interface. Decide stack ownership before building service machinery. Do not instantiate a second stack in a console, profile, runner or observer.
- Handle unsolicited events independently of an active request; preserve node/channel/binding/configuration generations. CANopen has no wire transaction ID for ordinary SDO transfers. A local token cannot prove a late response belongs to a new identical request; the owner must prevent ambiguous channel reuse and report unresolved cases.
- Preserve raw SDO abort (all 32 bits), CAN controller evidence, statusword/controlword and EMCY data. Distinguish local validation failure, transport failure, valid protocol rejection, driver fault and operation uncertainty.
- Never retry uncertain side effects automatically. Document controller link-layer retransmission versus application replay and ensure queued pre-recovery frames cannot execute later as fresh intent.
- Health/status/result inspection is cached; explicit reads refresh. `stats reset` affects counters only. `recover` affects host transport only. `cancel` does not stop the drive. Motion and persistent/network changes remain explicit.
- Keep common functions usable from native C++, Arduino, ESP-IDF, FieldCore-like owners and other frameworks. No mandatory FieldCore, Python, CANopen stack or SDK dependency in the exported core.
- Keep the full documented native family surface in a ledger and expose typed operations, not just a raw dictionary escape hatch. Unsupported firmware/unknown behavior is recorded as an unresolved feature, not silently counted as complete.
- Preserve copied MIT attribution. Vendor/standards material is reference-only and excluded from software license and distributable core bundles.

## Verification and report

Run the stage's focused checks and affected CI commands. Add meaningful positive and negative tests for new behavior, concurrency/late-event handling, capacity and uncertainty. Never claim a fake node test proves real drive behavior. Repeat/broaden checks only for changed code or unresolved concerns.

Write `docs/reports/stage_NN.md` containing:

```text
Stage / status / date / implementation revision or working-tree identifier
Inputs: sibling revisions + dirty states + actual files inspected
Source evidence: document IDs/pages + EDS sections + conflicts resolved/open
Decisions: naming/ownership parity and deliberate differences
Changes: public API, behavior, ownership, capacity, CLI/protocol impact
Checks: exact commands + results + artifacts; separate skipped/blocked checks
Coverage: documented / implemented / host tested / hardware qualified
Open issues: consequence, dependent stage, precise evidence needed
Handoff: next executable stage and any feature-specific gates
```

Update `docs/backlog.md`, `docs/roadmap.md`, capability/object/API/CLI coverage and `docs/prompts/PROGRESS.md`. If a stage is partially blocked, finish independent items and keep it partial. Do not call the whole library complete because the prompts ran, a build passed, or a feature returns UNSUPPORTED. Do not require hardware to complete host-only stages.

For multi-agent implementation, delegate independent read-only reviews or disjoint files, declare ownership, and let the lead integrate shared contracts/build registration/Git changes. Run final checks against the integrated result. Avoid concurrent edits to generated catalogues or central API headers.
