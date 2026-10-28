# Prompt 44 Finite Python campaigns and sessions

Apply [COMMON.md](COMMON.md). Prerequisites: [43](43_python_console.md).

## Objective and boundaries

Complete the six-file runner family with finite scenario planning, motion/action campaigns, bounded repetition and evidence-owning sessions. Reuse the existing `Console` and pure evidence helpers. Python orchestrates public CLI operations; it must not become another motion implementation or contain private controlword/object sequences. Complete host tests and dry-run behavior without assuming a physical bench. Actual campaign qualification follows in prompts 47–48.

## Read and audit

Read current RS `bench_motion.py`, `bench_repeat.py`, `bench_scenarios.py`, `bench_session.py`, their tests and shared client usage. Refresh ADS1115/INA228/SHT3x/PCA9555 finite HIL options, parser discipline and evidence verdict distinctions; inspect FieldCore's result semantics where affected. Read actual CAN CLI capability/schema and prompt 43's client tests. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

### Concrete evidence to recheck

Build scenario expectations from [motion](../audit/LICHUAN_MOTION_FACTS.md) and [object](../audit/LICHUAN_OBJECT_FACTS.md) evidence. Never assume EDS defaults are the fixture's starting state, convert LED's five-second diagnostic cycle into a timeout, or insert reset/save to make scenarios reproducible. Capture effective heartbeat/map/scale, firmware/dialect, requested parameters and host timing policies. Use exact homing method/input prerequisites and preserve raw evidence when a source claim disagrees with the device.

## Implement

1. Implement `bench_motion.py` for finite movement/action campaigns; `bench_repeat.py` for finite repetition of a named scenario; `bench_scenarios.py` for named plans, prerequisites, execution and expected evidence; and `bench_session.py` for session orchestration/artifacts/cleanup. These import `Console` from `bench_probe.py` and interpretation from `_bench_evidence.py`. Keep planning helpers importable; extract another module only for a demonstrated readability/reuse need.
2. Provide applicable finite read-only, position, stop, velocity, homing and settings scenarios through the public CLI. Validate selected protocol/profile/model/firmware/capability, configured scale/reference, I/O and stop prerequisites. Unsupported scenarios are explicitly skipped or rejected according to their purpose, never fabricated passes. Do not assume all nodes use 1..127, heartbeat/statusword semantics or integer-native targets.
3. Publish audited required arguments, finite duration/count/deadline limits and dry-run plans. Require explicit live port/interface/configuration. No automatic upload on import, endless repeat, connection-triggered motion or guessed settings. `--help`, imports, plan mode and unit tests remain hardware-independent with optional serial dependencies.
4. Never retry an uncertain move/save/reset or silently resume after reconnection. A finite best-effort stop or cleanup is a separate preplanned explicit operation with its own result. Failed/uncertain primary work remains failed/uncertain even when later cleanup succeeds; attempted cleanup does not prove standstill or restoration.
5. Reserve and retain evidence artifacts without overwriting prior failures. Record software/firmware/SDK identity, intent, IDs, selected dialect/configuration, relevant raw console/CAN evidence, owner/TX outcome, deadlines/service gaps, faults, resource/drop counts and physical observation only when present. Keep bounded summaries separate from streamed logs and preserve unsuccessful stop evidence.
6. Register campaign/session tests and deterministic report packaging with the existing verifier. Failures exit nonzero. A plan-only run has its own verdict; it never claims live observations. Document artifact paths and avoid collecting unrelated machine information.

## Verify and exit

Run fake-console campaigns for all supported scenario classes and prerequisite rejections. Simulate wrong profile, stale completion, duplicate response, boot mid-move, lost reply after TX, disconnect, serial-open failure, interrupted session, late/failed stop and evidence-write failure. Assert finite termination, preserved primary failure, no automatic replay and exact integer handling. Test repeat bounds, dry-run isolation and session output collision behavior. Run import/compile checks and the registered Python suite without hardware. Complete every runner's usable documented role; do not create empty script shells merely to match filenames.

## Documentation and delegation

Update the runner guide and `docs/bench_scenarios.md` with actual flags, capability prerequisites, finite commands, verdicts, exit codes and failure interpretation. Add reusable function/module docstrings and CHANGELOG entries. Normal engineering agents can own disjoint campaign tests or scenario prerequisite review after client contracts are fixed. Use stronger independent review for cleanup/replay uncertainty; one writer owns shared session behavior. Follow [DELEGATION.md](DELEGATION.md) and let the lead integrate tests and Git checkpoints.

Close this prompt's owned scope and checks; record `docs/reports/prompt_44.md` with host evidence and pending live qualification separately. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
