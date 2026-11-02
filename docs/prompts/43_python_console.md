# Prompt 43 Python Console and evidence primitives

Apply [COMMON.md](COMMON.md). Prerequisites: [40](40_cli_parser.md).

## Objective and boundaries

Deliver a tested Python client for the real console protocol, pure evidence interpretation and a finite explicit probe/read invocation. Implement `bench_probe.py` and `_bench_evidence.py` with the established RS responsibilities. Campaign, repetition and session orchestration follow in prompt 44. This prompt runs offline/fake tests unless a separate applicable hardware instruction exists; connection must never imply movement or reconfiguration.

Offline scope after prompt 40: the `Console` client, framing, handshake and correlation, pure evidence interpretation, fake streams and the offline plan and self-test entry points need only prompt 40's parser and console schema. The finite probe and read invocation (step 5) calls the probe and read routes that prompt 41 delivers. Until those routes exist, implement and test that invocation against the schema and fakes, keep it Blocked for live use, and name the missing route in the report; do not invent a route to unblock it.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read RS `bench_probe.Console`, handshake/correlation helpers, `_bench_evidence.py`, parser tests and verifier registration; RS is authoritative. The RS `@ID` protocol and `_bench_evidence.py` govern. Check HEAD and dirty state only for the siblings this prompt names: compare the ADS1115 contract and evidence verdicts, the PCA9555 dry-run guard and the INA228/SHT3x exact-version flags. I2C command framing is not a model, and `EVIDENCE_REQUIRED` exists only in ADS1115. Inspect FieldCore-node read-only for ownership or integration changes. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read the implemented CAN CLI schema and application output rather than an old example. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

### Concrete evidence to recheck

Use the [public implementation audit](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) as a negative design reference: fixed sleeps, shared global motor status and missing acknowledgement are insufficient correlation. Python consumes the established console protocol, not a separate copied CAN driver. If pinned canopen-python is used in a separate diagnostic tool, its retries/timeouts must not leak into automatic motion replay. Distinguish command acceptance, terminal result and raw device evidence in transcripts.

## Implement

1. Keep shared `Console` framing/handshake/correlation in `bench_probe.py`, with pure interpretation/summary helpers in `_bench_evidence.py`. Provide injectable fake streams/time for tests. Keep imports and `--help` usable without serial packages or hardware; load optional serial dependencies only in live paths.
2. Validate library/profile/protocol/version and relevant firmware/capability identity before requests. Preserve protocol/profile-specific address meaning; CAN bitrate/node configuration is not a serial-console port setting. No default COM port, guessed motor scale or assumed enabled state is acceptable.
3. Maintain bounded explicit correlation: request ID, admitted operation ID, firmware boot/session epoch, expected record kind, deadline and terminal state. Handle startup banners, unrelated events, delayed/duplicate lines, stale completion, malformed/truncated JSON and disconnects. Exact integer strings remain exact through decoding and summaries.
4. After framing loss, reboot or disconnect, retain interrupted/unknown outcome according to available evidence. Do not reconnect and replay an uncertain operation. A local request token cannot turn a late old device response into new acceptance; reflect the firmware's actual retained evidence. Never substitute a runner timeout for confirmed motor stop.
5. Add finite explicit read/probe and offline plan/self-test entry points using implemented public CLI routes; the probe and read routes come from prompt 41. Validate all arguments before opening a port. Audit actual RS argument spellings and reuse them only where meanings agree. RS `bench_probe.py` has no plan or self-test mode: `--list` exists only on `bench_scenarios`, and `--plan-only` is on the campaign tools with a read-only default `--scenario quick`. State the CAN spellings explicitly and give plan mode a read-only default scenario. RS `bench_probe --port` is required; do not copy the default `COM13` that `bench_motion`, `bench_repeat` and `bench_scenarios` use, or a default address 1. Document explicit port/profile/node/configuration and timeout bounds; a future direct-CAN client is a separate adapter.
6. Retain bounded raw received lines, parse failures, intent, IDs and structured evidence plus a readable summary. Distinguish planned/skipped/host-tested/live-observed results, pre-TX failure, native rejection and uncertain effects. Preserve raw protocol/controller information and avoid including unrelated host secrets in artifacts.

## Verify and exit

Register `*_test.py` with the actual verifier/CI. Run fake-stream tests for split UTF-8/lines, bad JSON/types, duplicate keys where relevant, oversized records, wrong protocol/version, mismatched IDs, stale completion, reboot and lost reply after admission. Assert bounded memory/time, finite termination, nonzero failure exit and no replay. Run import/compile checks and safe `--help`/plan commands without installed serial dependencies. Test pure evidence interpretation independently of I/O. This establishes host automation primitives, not a passing live campaign.

## Documentation and delegation

Write module/function docstrings for reusable client/evidence contracts, setup/dependency pins, exact offline/live invocation syntax, exit codes and evidence interpretation. Update `docs/runners.md` (the runner guide), the source map and CHANGELOG. A normal engineering agent can own pure evidence tests after the schema is fixed; a stronger reviewer challenges correlation/epoch/uncertain outcome cases. One writer owns the client protocol and lead integrates actual verifier registration. Keep Git and hardware activity out of worker scope.

Close this prompt's owned scope and checks; record `docs/reports/prompt_43.md` and the campaign boundary. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
