# Prompt 44 finite campaign source audit

Checked 2026-11-14 against local sibling source and CAN prompt-entry commit
`9e036f6`. Sibling reads were read-only. These are local revisions, not upstream
synchronization claims. Every listed sibling HEAD advanced since prompt 43;
the campaign seam was read at the revisions below. No hardware ran, no vendor
snapshot changed, and this audit adds no device timing or capability claim.

## Refreshed sources and disposition

| Repository | HEAD / working tree | Inspected seam and decision |
| --- | --- | --- |
| MotorControl-RS | `13cf955eeee278db9c01fd7acff1bedb0e516fb5`, clean | `scripts/bench_motion.py::phase_plan`, `run_phase`, `_run_phase`, `profile_snapshot`; `scripts/bench_repeat.py::campaign`; `scripts/bench_scenarios.py::arguments`, `run_scenario`, `_setting`; `scripts/bench_session.py::run_recorded`, `snapshot`, `cached_failure`. Adopt finite public-operation plans, one session owner, scenario-owned cleanup, exclusive streamed evidence and bounded summaries. Reject ESS register semantics, fixture defaults, automatic recovery and unsupported CAN restoration/policy assumptions. |
| ADS1115 | `e07622a99fe9f13c5d6a5a2497a8112a14185b9d`, clean | `tools/run_i2c_hil.py::parse_args`, `argument_validation_error`, `final_verdict`, `contract_verdict`, `evidence_verdict`, `process_exit_code`, `main`. Adopt distinct contract and physical-evidence verdicts and a no-port dry run. Reject I2C commands, address defaults, serial reset and exploratory-success treatment of missing CAN evidence. |
| INA228 | `4de84f2564eb6382835ca9a72d8688377657dcb4`, clean | `tools/run_i2c_hil.py::classify_step`, `run_step`, `main`; `tools/test_run_i2c_hil_parser.py::test_serial_commands_require_full_write_without_host_drain`. Adopt complete framed results, short-write failure and cache-health capture distinct from readiness. Reject sensor recovery, unbounded host drain and its `UNKNOWN` exit override as CAN policy. |
| SHT3x-main | `7d04cd1a94a01eb9c144fd95d4ec95cff3d22ec3`, clean | `tools/run_sht3x_hil.py::validate_parsed`, `run_dry`, `verdict`, `verdict_exit_code`, `parse_args`, `main`; parser tests `test_dry_run_verdict_is_incomplete`, `test_nonpass_verdicts_are_nonzero_without_explicit_override`. Adopt finite duration validation, precise identity expectations and separate skipped/operator evidence. Reject sensor reconnect/recovery and copying firmware fields absent from CAN. |
| PCA9555 | `8b461b2afe9910b5b15fe32cc049d52e3b498970`, clean | `tools/run_i2c_hil.py::validate_args`, `final_verdict`, `exit_code_for_results`, `run`; `tools/test_run_i2c_hil_parser.py::test_dry_run_artifacts_include_classifier_and_timing_options`. Adopt no serial import/open in dry run, retained timing policies and explicit incomplete verdict. Reject serial-open retries, arbitrary command campaigns and assuming its permissive operator-review exit means CAN work passed. |
| FieldCore-node | `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean | `src/devices/rs485_motor/Rs485MotorModule.cpp::applyRuntimeConfig`, `startMotor`; `src/rs485/Rs485Task.cpp::applyPendingConfigs`. Adopt application ownership, no motor retries, retained binding/transport tuple until safety-lane settlement, and retirement before configuration mutation. Reject sensor retry inheritance and treating this RS485 integration as CAN support. |

## Actual RS runner contracts

| Source | Audited arguments and bounds | CAN consequence |
| --- | --- | --- |
| `bench_motion.py` | Required `--phase`, `--out`; `--plan-only`; optional `--debug`, `--address`, `--declare-unconnected`. Port defaults to `COM13`, address to 1; ESS range is 1..247. Nine named phases; motion running observation at most 32 reads; event summary tail 32. Snapshot read/restore has an explicit deadline in (0,60] seconds and at most 101 command turns, with passive `inspect` after the first request. | Reuse phase planning and single-attempt intent. Require explicit CAN route facts. Do not copy raw position windows, ESS units, restore commands or terminal declarations. |
| `bench_repeat.py` | `--count` 1..20, default 10; `--out`, optional `--firmware`, `--plan-only`; port defaults to `COM13`. Schedule is `3*count+3` ordinary moves comparing `write`, `verify`, `stored`, followed by one stop, bounded stopped observation, one same-session profile restoration and ten probes. It accepts no arbitrary scenario. | Bound comparison and plan one cleanup. CAN `stored` is gated, so only compare policies implemented by its public path. No repeat of settings, persistence or restore. RS latency is acknowledgement timing, not physical shaft latency. |
| `bench_scenarios.py::arguments` | Only this runner has `--list`; default `--scenario quick`. Quick/load/observations default to ten samples and 0.05 seconds; count 1..100, interval 0..1 seconds, timeout (0,60] seconds. Other scenarios reject count/interval options and default to one attempt. Evidence records 2..200000, default 20000; MiB 1..64, default 16. Non-plan live runs require `--out`. | Adopt read-only default, scenario-specific validation and plan mode before port/artifact creation. RS names are reusable only with matching CAN meaning. |
| `bench_session.py::run_recorded` | Owns `Console`, one port, `.json` summary and `.jsonl` stream. Both files use exclusive creation before opening the port. `run(console, record)` owns scenario cleanup. Summary retains the last 16 campaign records plus independent total/failure counters. Success requires `workload_verified is True`; close failure turns success into failure. | Preserve failed work even after successful cleanup and after summary-tail eviction. Do not put generic motor-stop logic in the session. |
| `bench_probe.py::observe_stopped` | At most ten reads, intervals 0..0.05 seconds, one overall deadline. `drive_reported_stopped` requires successful state evidence, no alarm, non-running and raw speed zero. | Reuse bounded observation structure through CAN helpers, not RS fields or a physical standstill claim. |

RS `snapshot(refresh=True)` reads identity, configuration and state before and
after successful work. CAN must not copy this unconditional configuration read:
its ordinary configuration acquisition is Q20-gated. Failure snapshots in RS
use cached diagnostics only and stop at the first failed diagnostic.

RS reserves the summary before the JSONL file; a pre-existing JSONL file can
leave a newly created empty summary after reservation fails. Preserve existing
evidence and track which artifacts belong to the attempted reservation. Its
`bench_repeat.campaign` retains the primary failure in `record['failure']`, but
a cleanup exception from `finally` can become the surfaced exception; CAN should
retain primary and cleanup failures separately.

## Three helper names are not transferable protocol rules

| Exact RS symbol | Actual behavior | Decision |
| --- | --- | --- |
| `scripts/bench_probe.py::cleanup_stop_attempts` | Sends one stop; a refused reply causes one `recover`, then one fresh stop if recovery succeeds. An accepted stop is not resent. `move_campaign` retains the ordered list as `cleanup_attempts`. | Retain ordered cleanup evidence where useful. Reject implicit recovery and second stop: CAN prompt 44 plans one explicit cleanup operation and never reconnects or replays uncertain work. |
| `scripts/_bench_evidence.py::write_rejected` | Accepts Modbus exception codes 1..7 except 5, a documented read-range error. Used by motion/home evidence validation to distinguish refused writes from uncertainty. | Reject this predicate for CAN. Preserve CAN terminal status, unsigned SDO abort and owner/TX evidence through the existing parser. |
| `scripts/_bench_evidence.py::home_inputs_permit` | Applies ESS input function numbers: active path-incompatible limits reject except method 35; active release/stop/emergency stop reject; motion triggers must be inactive and declared unconnected. `check_home` applies it to copied qualification and subsequent raw observations. | Reject ESS numbers and wiring meanings for CAN. Use its public homing prerequisite path and model-applicable evidence. A generic helper must not invent CAN homing permission. |

## CAN contracts and checks read

Read `examples/common/ProbeConsole.cpp` command inventory and profile output,
`docs/cli_contract.md`, `scripts/bench_probe.py` and the prompt-43 tests
`test/bench_console_test.py`, `test/bench_console_adversarial_test.py` and
`test/bench_probe_cli_test.py`. The CAN schema retains uint32 request/operation
identities and decimal strings for wide integers/timestamps. Profile output has
unknown model/firmware matches and `hardware_qualified:false`; handshake cannot
qualify a drive or movement. Lost admission, lost terminal, short write, duplicate
response, post-handshake boot and evidence-write failure fence further I/O.

DEC-1 accepts the explicit probe `1000:00`, identity and state
STATUS/MONITORS/FEEDBACK plans. Configuration, other state blocks, native settings
and motion-profile acquisition keep Q20 whole-plan rejection before traffic.
The first active campaign read must use the same gate; a needed unavailable read
must produce a named skip/rejection. CAN `motion stored` and `motor-release`
remain gated. Python cannot replace firmware qualification, acquire a missing
scale/reference or use a raw controlword/object fallback.

RS tests were read, not executed: `test/motion_campaign_test.py` covers bounded
standstill, one cleanup, lost framing, failed/uncertain stop, restoration and
repeat bounds; `test/bench_scenarios_test.py` covers invalid crossed options,
plan isolation, settings uncertainty, one owner, collision, close failure,
evidence exhaustion and primary failure retention. RS CMake registers
`motion_campaign`, `bench_scenarios` and `bench_probe`; its verifier checks actual
Python CTest commands. CAN registers Python tests in `test/CMakeLists.txt` and
checks them through `scripts/check_repository.py` using the CTest inventory from
`scripts/verify.py`. `.github/workflows/verify.yml` uses Python 3.12; quick and
full verifier commands remain the integration gates. Package/report additions
must use the existing verifier and deterministic packaging contracts.

This audit is source evidence only. The prompt report owns implementation,
combined verification, Git synchronization and pending live qualification.
