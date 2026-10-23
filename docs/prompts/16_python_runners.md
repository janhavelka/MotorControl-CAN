# Stage 16 Implement Python runners and reproducible evidence

Apply [COMMON.md](COMMON.md). Prerequisite: stage 15.

## Audit

Read RS `bench_probe.py`, `_bench_evidence.py`, `bench_motion.py`, `bench_repeat.py`, `bench_scenarios.py`, `bench_session.py`, parser/campaign tests and verify registration. Compare INA228/ADS1115/SHT3x/PCA9555 runner/parser conventions and finite HIL options. Reuse exact established flags where their meaning applies; do not invent identical runner behavior across siblings that differ.

## Deliver

1. Create the corresponding script family with separate session/framing/correlation, pure evidence interpretation, finite scenario planning and campaign orchestration. Keep imports usable by tests, `--help` safe and serial dependencies optional/lazy for host parser tests. Do not reimplement motion in Python.
2. Validate firmware library/profile/protocol/version handshake before commands. Correlate command IDs, operation IDs, terminal records and retained results; handle boot banners/epochs, unrelated events, delayed lines, duplicates, malformed/truncated JSON, disconnects and protocol mismatch.
3. Make all hardware activity explicit via port/interface/node/bitrate/configuration arguments. No default COM13, upload on import or automatic motion on connection. Provide finite probe/read-only, position, stop, velocity, homing and settings scenarios only where prerequisites/capabilities permit.
4. Never retry an uncertain move/save/reset automatically. Preserve failed evidence and stop/reconciliation result; an attempted cleanup is not a confirmed stop. Bound each command/campaign and ensure failures exit nonzero. Reconnection does not silently resume a campaign.
5. Retain structured JSONL plus readable summaries: firmware/library/SDK revisions, command intent, IDs, selected profile/dialect/config, raw relevant CAN traffic, owner/TX evidence, deadlines/service gaps, accepted/executed/unknown state, faults, resource/drop counts and actual physical observation when available.
6. Add dry-run planning and fake console tests, scenario inventory, exact invocation examples and dependency pins. Register `*_test.py` in the verifier/CI. Keep report packaging deterministic enough to inspect and avoid recording unrelated machine secrets.

## Concrete implementation contract

Use the RS script family with clear responsibilities:

| File | Responsibility |
| --- | --- |
| `bench_probe.py` | Shared `Console` framing/handshake/correlation helpers and bounded explicit read/probe invocation |
| `bench_motion.py` | Finite movement/action campaign using public CLI commands |
| `bench_repeat.py` | Finite repetition of a named, prerequisite-checked scenario |
| `bench_scenarios.py` | Named finite scenario planning/execution, requirements and expected evidence |
| `bench_session.py` | Finite session orchestration, evidence and cleanup; imports shared `Console` helpers |
| `_bench_evidence.py` | Pure evidence interpretation and summary helpers |

Do not create a second motion controller in Python. The serial port is the host-to-example console transport; CAN bitrate and node selection are device-network configuration carried explicitly through the console. A future direct CAN client is a separate adapter, not an interchangeable meaning of the same port argument.

This split follows the current RS source: `Console` lives in `bench_probe.py`, while `bench_session.py` uses it and `bench_scenarios.py` runs campaigns. Keep pure planning/evidence helpers importable within those files. A later extraction needs a concrete reuse/readability benefit and coordinated imports/tests, not a new generic runner framework.

Audit actual RS argument names and select matching flags where semantics agree. Publish a command table with required port/profile/node/configuration, duration/count bounds and scenario prerequisites. No implicit COM port, default motor scale, guessed enable state or endless repeat. Optional serial imports occur only in live paths; `--help`, plan mode, unit tests and imports work without hardware.

Keep correlation state explicit and bounded: request ID, operation ID, firmware boot/session epoch, expected record kinds, deadline and terminal status. Record `uncertain` distinctly from failure before TX. On disconnect or reboot, close affected requests as interrupted/unknown using available evidence; do not reconnect and silently replay. A finite best-effort stop is a separate explicit operation and cannot rewrite the original result.

Evidence files retain command intent, received records, parse failures and scenario summary. The summary says planned/skipped/host-tested/live-observed separately. A plan-only run is never a HIL pass. Exact native integer strings stay exact throughout parsing and summaries. No private runner-only command bypasses CLI capabilities.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Document setup, offline commands, finite live commands, exit codes, evidence paths and failure interpretation. Add Python module/function docstrings for reusable session/evidence/scenario contracts; Doxygen remains required for any changed C++ interface.

Use [DELEGATION.md](DELEGATION.md) for parser/evidence tests and scenario prerequisite review after schema agreement. One writer owns the session protocol. Run tests through the registered verifier with fake streams, including split lines and lost replies after admission.

## Verify and exit

Run parser/campaign tests without hardware and py_compile/import checks. Simulate wrong protocol, stale completion, duplicate command response, reboot mid-move, lost reply after TX, partial UTF-8/line framing, bad exit code, serial open failure and late stop. Assert no replay and finite termination.

Exit with working host automation, not merely CLI examples. Live campaign qualification remains stage 18.
