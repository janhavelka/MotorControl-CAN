# Requirements and acceptance matrix

These are obligations for the future implementation, not current feature claims. The planning package supplies the audits, references and stages; `docs/prompts/PROGRESS.md` starts pending. Every stage must update traceability to real code/tests/evidence as it executes.

| ID | Requirement | Prompt owners | Acceptance evidence |
| --- | --- | --- | --- |
| R01 | Thorough live MotorControl-RS audit | 1-2; recurring all | Exact source/API/CLI/owner/test/package inspection, HEAD/status and dated changes |
| R02 | Audit current I2C conventions every prompt | All | Stage report names actual relevant source/callers/tests/runners/CI; differences recorded |
| R03 | Same repository responsibilities and direct naming | 5-10, 40-42, 49-50 | Source layout and API/CLI map reconciled to actual RS; no invented generic getters/old aliases |
| R04 | Portable C++11 core and native consumers | 5-12, 15, 17, 26-28, 45-46, 49-50 | Source/install/export/header builds without framework, SDK, Python, stack or sibling requirement |
| R05 | Caller-owned contexts/storage; application transport | 5-6, 13-18, 23-25, 45-46 | Explicit lifetimes/capacities, no core I/O/clock/heap/tasks, one receive owner |
| R06 | Initial reviewed Lichuan C family within a multi-drive library | 2-4, 19-22, 29-36, 47-50 | Per-model/firmware/dialect caps, full native coverage ledger, independent hardware labels |
| R07 | Original manuals/EDS and provenance | 1-4 | Local originals/extracts/hash manifests and exact object/page evidence; no invented chip |
| R08 | Resolve contradictory vendor behavior | 2-4, 19-22, 29-36, 47-48 | Documented decision or explicit affected-feature gate; no union of status bits |
| R09 | Exact units/coordinates and ordinary motion names | 9-10, 21-22, 32-36 | Boundary vectors, no float loss/native-width leak, explicit scale/origin/rounding |
| R10 | Lossless status/protocol/drive/outcome/health separation | 9-10, 13-25, 30-32 | Raw abort/EMCY, state/freshness and uncertain outcome tests; cached diagnostics |
| R11 | Correct CANopen service/network lifecycle | 11-18, 23-25 | Actual SDO forms required by devices, independent event dispatch, PDO map epochs, boot-up handling |
| R12 | No automatic replay or accidental actuation | 13-14, 21-25, 30-39, 45-46 | Late-reply ambiguity, SDK queue retirement, no startup/probe/recover side effects |
| R13 | All confirmed native functions have typed access | 2-4, 19-22, 30-39, 49-50 | Ledger -> typed API -> implementation -> test; raw SDO alone insufficient |
| R14 | Common enable/release/fault/stop behavior | 21-25, 29-36, 47-48 | Observed state and uncertainty; stop priority, no NMT/host-cancel equivalence |
| R15 | Drive-generated position/velocity/homing | 21-22, 29, 32-36, 47-48 | Trigger/acceptance/new completion evidence, finite deadlines and exact physical scope |
| R16 | Non-changing discovery and explicit commissioning | 37-39 | Bounded reads, uncertain identity, no network writes hidden in discovery |
| R17 | Native plus shared Arduino/ESP-IDF examples | 26-28, 40-42, 49-50 | Common app/CLI/public APIs, compiled adapters with explicit pins/SDK and fake transport |
| R18 | Consistent CLI and machine protocol | 40-44 | One inventory/help, strict parser, request/operation IDs, schema/version tests, no private motion path |
| R19 | Python finite runners and retained evidence | 29, 43-44, 47-48 | Offline parser/campaign failures/no replay, explicit connection/setup, logs and summaries |
| R20 | Native/fault/consumer/package/CI gates | 7-28, 30-46, 49-50 | Registered *_test.cpp/*_test.py, GCC/Clang, package users, exact-commit CI when available |
| R21 | FieldCore-like suitability without dependence | 1-2, 5-6, 23-25, 45-46 | Read-only current dormant module audit, fake consumer, handoff; no imported app types |
| R22 | Agent guidance and resumable delivery | All | One new branch per new promptset execution, reused across all stages/sessions; commit and sync each prompt/coherent checkpoint; reports preserve evidence and open obligations |
| R23 | Documentation and release honesty | All; final 49-50 | Documentation accompanies changes; supported versus known/implemented/tested/qualified matrix; no fabricated firmware/HIL |
| R24 | Maintainability and resource boundaries | All; architecture 5-6 | Selected single stack, compact linked metadata, bounded queues/work, no universal engine |
| R25 | Correct package/license provenance | 7-8, 49-50 | Copied MIT notice, optional stack license, third-party originals excluded from software export |
| R26 | One library for multiple vendors, stepper and servo | 1-10, 45-46, 49-50 | Capability/motor/feedback separation; N5 stepper+BLDC and EPOS4 contrast evidence before API freeze |
| R27 | Common motion independent of CANopen | 5-6, 9-18, 23-25, 45-46 | Logical axis/subaxis identity; native network/state/ack semantics optional; no universal node-ID range |
| R28 | Multiple CAN protocol/frame requirements | 5-6, 11-12, 15, 17, 23-28, 45-46 | 11/29-bit/RTR framing, FD capability rejection, protocol-specific routes and mixed-bus conflict policy |
| R29 | Servo/native units, effort and completion semantics | 2-4, 9-10, 21-22, 30-36, 45-46 | No fixed RPM width/full-step requirement; integer/float quantization, command/feedback and current/torque distinction |
| R30 | Verifiable new-drive extension path and support scope | 5-6, 29, 45-50 | Test-only documented contrasts; profile-subset builds; ADD_DRIVE procedure; research is not implemented support |
| R31 | Concrete source-fitted conventions | All; 1-2, 9-10, 23-25, 40-42 | CONVENTIONS and fresh audit distinguish observed versus proposed names; exact Status/outcomes/owner/Health/Stats and counter/reset tests |
| R32 | Authorized subagents and model selection by complexity | All | Runtime-supported model/effort choices, bounded tasks, disjoint ownership, independent difficult-contract review and integrated verification |
| R33 | Continuous general docs and in-source Doxygen | All; tooling 7-8, final 49-50 | README/CHANGELOG/CONTRIBUTING/SECURITY/AGENTS and affected guides; units/lifetime/timing/failure comments; strict nonempty API docs and compiled examples |
| R34 | Simple readable maintainable engineering | All | Direct functions/explicit state; justified new abstractions, no duplicated owners/frameworks; clear names, source rationale and plain language |
| R35 | Concrete stage work and resumable acceptance | 1-50 | Every prompt names artifacts/API behavior, failure cases, docs and useful work split; coverage rows have owning stages; reports preserve unresolved obligations |

## Required failure cases

The final review must be able to locate tests for these behavior groups, not only count passing suites:

- Frame flags/length/COB-ID, object address/type/sign/width, SDO abort, malformed transfer size and unchanged output on error.
- SDO timeouts before/after TX, late identical responses, segmented toggle/length/capacity where used, cancelled channel and reboot.
- PDO unknown/changed mapping, partial remapping, stale generation, EMCY during a transfer, heartbeat disabled/missed/restored and absent host-loss guarantee.
- Queue admission/result exhaustion, fairness, urgent stop, real pending-TX retirement, bus-off recovery, receive overflow and slow console/debug observer.
- Unknown identity/dialect, mode/control/status disagreement, unsafe stale target at enable, trigger/ack/completion separation and prior completion-bit reuse.
- Exact arithmetic extremes, explicit native/ramp units, invalid scale/origin, angle ties, range limits and generation/freshness invalidation.
- Partial setting/persistence/communication effects, failed readback, no automatic save/factory reset, explicit I/O assignment and no undocumented retry.
- CLI truncation/overflow/trailing junk, schema/version mismatch, interleaved JSONL, duplicate IDs/results, reboot during campaign and no command replay.
- Multi-vendor stepper/servo configuration without changing common motion types, optional native states/acknowledgements, multi-axis logical bindings, alternate rate/feedback scales, float-native quantization, extended/RTR admission, FD rejection and overlapping protocol routes.

## Final support language

“Documented” means a cited source describes it. “Implemented” means a real callable path exists. “Host tested” means relevant native/adapter/parser tests passed. “Firmware built” means the exact SDK/platform built. “Hardware qualified” means the model/firmware/configuration/fixture and actual observations are recorded. These are separate columns; none implies the next.

The architecture includes multiple manufacturers' stepper and servo drives and separate CAN protocol extensions. The first production implementation remains Lichuan Classical CANopen. Surveyed N5/EPOS4 and proprietary protocols are contract references/candidates until a real profile is implemented. Initial operation scope excludes generic host-streamed cyclic/effort control, synchronized multi-axis guarantees, non-CAN interfaces and safety certification. Features within the documented selected implementation scope remain in the ledger even when source contradictions currently prevent implementation. They must not disappear from the final completeness claim. Apply the mandatory [multi-drive stage criteria](prompts/MULTIDRIVE.md).
