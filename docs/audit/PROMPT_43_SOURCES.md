# Prompt 43 Python Console source audit

Checked 2026-11-12 against local sibling source and the CAN console at prompt
entry (`3249ee8`). All sibling reads were read-only; no sibling tests, hardware
activity, Git mutations or vendor claims were introduced. Revisions below are
local snapshots, not upstream synchronization claims. The implementation report
owns final combined checks and any schema additions made during prompt 43.

## Refreshed sources and delta

| Repository | HEAD / working tree | Inspected behavior and disposition |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | Revision/state unchanged since prompt 42; console inventory unchanged since prompt 40. Expanded this prompt's audit to `scripts/bench_probe.py::Console`, `Command`, `Evidence`, `identify`, `arguments`; `scripts/_bench_evidence.py`; `test/bench_probe_test.py`; `scripts/verify.py::check_inventory`, `CMakeLists.txt` and `.github/workflows/verify.yml`. Adopt single Console ownership, `@ID` framing, explicit admission, retained operation correlation and no replay. Reject RTU windows/CRC, address and console defaults, device timing and family-specific terminal schemas. |
| ADS1115 | `d613a13cd3d43471905c507bfb4094bba1c1da94`, clean | Revision/state unchanged since prompt 40; runner verdict seam newly inspected at this revision. `tools/check_cli_contract.py::main` checks real example dispatch and runner contract. `tools/run_i2c_hil.py::classify_output`, `contract_verdict`, `evidence_verdict`, `final_verdict`, `process_exit_code` separate serial contract success from missing fixture evidence. Adopt separate evidence claims; reject sensor commands/framing and exploratory-success exit policy for a failed CAN invocation. |
| PCA9555 | `414e0f66b13082e974ec27a4a6e86985a65d83fd`, clean | Revision/state unchanged since prompt 32; dry-run seam newly inspected at this revision. `tools/run_i2c_hil.py::run`, `final_verdict`, `exit_code_for_results` keep dry-run outside serial import/open and return `INCOMPLETE`, never a physical pass. `tools/test_run_i2c_hil_parser.py::test_dry_run_artifacts_include_classifier_and_timing_options` checks plan artifacts without serial transcripts. Adopt the no-port dry-run boundary; reject sensor serial-open retries and its distinct verdict vocabulary as a universal contract. |
| INA228 | `73f954838d3fac8cce747d54d0ad738940d4c625`, clean | Revision/state unchanged since prompt 40; exact-version seam newly inspected at this revision. `tools/run_i2c_hil.py::version_contract_errors`, `run_step`, `main` and `tools/test_run_i2c_hil_parser.py::test_framework_and_provenance_contract` validate firmware version/commit/status. Adopt explicit identity checking; reject I2C `hilrun` framing and interpreting framework identity as drive identity. |
| SHT3x-main | `39d5b076960781cc5539e3ec3a37a96ffda743f7`, clean | Revision/state unchanged since prompt 40; exact-version seam newly inspected at this revision. `tools/run_sht3x_hil.py::validate_parsed`, `parse_args`, `main` and `tools/test_run_sht3x_hil_parser.py::test_version_requires_expected_clean_commit` check version, exact live commit and clean firmware. Adopt precise version expectations; reject copying different option spellings or fabricating firmware commit evidence absent from the CAN console. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Ownership/integration seam unchanged since prompt 39; not reread. `src/devices/rs485_motor/Rs485MotorModule.cpp::applyRuntimeConfig` rejects runtime address/baud changes; `src/rs485/Rs485Task.cpp` retires work before reconfiguration. Adopt application ownership; no CAN integration or FieldCore dependency is inferred. |

The older ecosystem audit used earlier sibling revisions. Its conclusions were
checked at the refreshed revisions for the specifically requested runner seams;
unchanged unrelated sensor modules were not reread.

## Actual RS responsibilities

`Console._lines/_decode/_read` bound input and preserve fragmented lines until
complete UTF-8 decoding. JSON duplicate keys and nonfinite constants fail through
`unique_object` and `invalid_constant`. Startup diagnostics are bounded and
separate from structured request replies. A framing, timeout, short-write or
disconnect failure poisons the session; later commands cannot replay work.

`begin` validates before send and creates one `Command` with its request ID,
immutable deadline, accepted flag, admitted operation ID and terminal. `_dispatch`
requires the matching ID, command, profile, record kind, original command ID and
accepted operation ID. Operation IDs cannot be reused after release. `wait`
routes interleaved traffic and preserves the terminal; an optional correlated
release is separate host work. `command` selects that release policy. A local ID
never establishes device-level wire correlation or a physical stop.

`identify` checks `product:"MotorControl-RS"`, protocol 3 and
`outstanding_capacity:10`. RS has no firmware boot/session epoch field here.
`_complete` notices regressing `status.uptime_ms`; boot/reset/panic lines also
invalidate an active session. Neither uptime nor a Python token is an immutable
firmware boot identity. CAN must describe missing epoch evidence honestly and
must not reconnect/replay an interrupted request.

`Evidence` streams to caller-supplied output with finite record/byte/record-size
limits, reserves an evidence-limit record and latches write/exhaustion failure.
`_bench_evidence.py` is pure supplied-record validation: it owns no port, clock,
session cache, command execution or release policy. Its shared `BenchError` and
helper identity are tested across supported imports. RS raw frame checks apply
to RS alone; CAN consumes its public console records rather than a copied driver.

Relevant adversarial tests include `test_single_byte_serial_fragments_keep_original_correlation`,
`test_json_duplicate_keys_and_nonfinite_values`, `test_line_and_total_byte_limits`,
`test_wrong_firmware_stops_before_probe`, `test_panic_reset_and_boot_stop_active_session`,
`test_disconnect_read_or_write_poison_session_without_recovery`,
`test_delayed_duplicate_detected_before_new_send`,
`test_missing_or_invalid_admission_operation_id_stops`, and
`test_missing_probe_terminal_does_not_retry` in `test/bench_probe_test.py`.

RS registers Python tests explicitly with CTest and verifies every
`test/*_test.py` against the actual CTest command inventory. Its `bench_probe`
test also consumes explicit native position-interruption fixture binaries.
Python 3.10 is its CMake minimum; CI uses Python 3.12 and the real quick/full
verifier. CAN's own registration is in `test/CMakeLists.txt`, with verifier
inventory enforcement; source-level test existence alone is insufficient.

## Exact argument and verdict differences

RS `bench_probe.arguments` requires `--port` and `--log`; it has no plan or
self-test mode. Its `--baud` is USB-console baud, while `--address` defaults to
ESS address 1. CAN must require the actual route facts and must not copy that
address default. RS `bench_motion` and `bench_repeat` use `--plan-only` and a
default `COM13`; `bench_motion` additionally requires `--phase`. Only
`bench_scenarios` supplies `--list` and `--scenario quick`, whose description is
read-only. CAN's offline spelling and read-only default are an explicit CAN
contract, not an existing RS `bench_probe` feature.

ADS1115 alone uses `EVIDENCE_REQUIRED`. Its contract verdict may pass while its
evidence/final verdict still requires fixture evidence; `--fail-on-unknown`
controls exploratory exit behavior. PCA9555's dry-run final verdict is
`INCOMPLETE`, and sensor runners use other distinct unknown/review labels.
These differences support keeping planned/skipped/host-tested/live-observed
evidence separate rather than importing one universal success enum.

INA228 flags are `--expected-library-version`, `--expected-commit` and
`--expected-git-status`; version/commit permit explicit `any`, and the default
status is `clean`. SHT3x uses `--expect-library-version`,
`--expect-library-commit`, and `--allow-dirty-firmware`; live entry requires a
nonempty version and exactly 12 hexadecimal commit characters. The spellings
and guarantees are different. Neither sibling supplies CAN firmware identity
fields which the CAN application does not emit.

## CAN schema rechecked before implementation

Read `examples/common/ProbeConsole.cpp` (version/profile/config dispatch,
`ProbeConsole::poll` and typed value output) and [the CLI contract](../cli_contract.md).
The established identity is `schema:"motorcontrolcan.console/1"`, `protocol:3`,
`profile:"lichuan_c"`, product `MotorControl-CAN`, generated version and
`operation_capacity:2`. This is not RS protocol-3 wire compatibility.

Request/operation/original-command IDs are uint32 JSON integers. Wide raw values,
timestamps and exact numbers are decimal strings; unknown is null. Local replies
use `type:"reply"`, admitted work has explicit `result:"accepted"`, reads emit
`type:"value"`, and final work emits `type:"terminal"`. Values retain object
index/subindex/width, raw and optional signed strings, units, currentness and
original time bounds. Terminals preserve independent status/detail, outcome,
uncertainty and raw abort evidence. `result ID` inspects retained evidence;
`release ID` is explicit. Unsolicited debug uses `type:"trace"`, `id:0`.

At prompt entry CAN emitted no boot/session epoch, uptime or firmware Git commit.
Its profile inventory explicitly had unknown model/firmware matches and
`hardware_qualified:false`. `config` reported host interface, CAN bitrate and
selected node; console baud is separate. Probe/read use the existing prompt-41
routes with DEC-1/Q20 policy. No handshake proves drive model, firmware support,
read harmlessness, scale, enabled state or hardware qualification.

The pinned public-implementation negative reference was rechecked in
[LICHUAN_PUBLIC_IMPLEMENTATIONS.md](LICHUAN_PUBLIC_IMPLEMENTATIONS.md) and
`docs/reference/public_code/srt/SRT_OpenCan.cpp::process_msg`, `enable_motor`,
`is_motor_running` (upstream `30ec0a617fdbbcb6d17b42edc85c5210344f6796`):
fixed sleeps, file-global motor status, missing checked acknowledgement and
blocking receive loops cannot establish correlation or completion. No public CANopen
Python stack is needed for this console client, and none authorizes retries.

This audit establishes source observations only. The lead owns final CAN schema,
client tests, documentation, combined validation, commit and synchronization.
Local audit checks passed: `python scripts/check_promptset.py` and
`git diff --check -- docs/audit/PROMPT_43_SOURCES.md`. These checks do not test
the Python client or qualify hardware.
