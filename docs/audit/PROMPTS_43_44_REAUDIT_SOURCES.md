# Prompts 43 and 44 source and coverage re-audit

Checked 2026-11-15 on `main`, starting at
`0a11ea6d205c03dbb27d5b7ff29b7a0c00be4724`. This independent review reads the
original numbered prompts and current implementations rather than treating the
earlier reports as acceptance evidence. The applicable Projects/root AGENTS,
COMMON, conventions, documentation, maintainability, delegation and multi-drive
contracts apply. This worker owns this report and the subsequently delegated
evidence-sink regressions in `test/bench_campaign_adversarial_test.py`. The lead
owns production code, shared documentation, Git synchronization and final
combined verification.
No sibling files, Git state, SDK, serial port or hardware were changed.

## Refreshed sibling evidence

Revisions are local snapshots, not upstream synchronization claims. Every
listed worktree was clean when refreshed.

| Repository | Current HEAD | Named seam and disposition |
| --- | --- | --- |
| MotorControl-RS | `13cf955eeee278db9c01fd7acff1bedb0e516fb5` | Campaign seam unchanged since prompt 44: `scripts/bench_motion.py::phase_plan`, `bench_repeat.py::campaign`, `bench_scenarios.py::arguments`, `bench_session.py::run_recorded`. Adopt finite public-operation plans, one session/port owner, scenario-owned cleanup and separate primary/cleanup evidence. Reject `COM13`, address 1, ESS units, unconditional configuration acquisition, restoration and recovery/retry policy. The Console seam was also checked directly at this revision, as explained below. |
| ADS1115 | `e07622a99fe9f13c5d6a5a2497a8112a14185b9d` | Unchanged since prompt 44: `tools/run_i2c_hil.py::argument_validation_error`, `contract_verdict`, `evidence_verdict`, `final_verdict`, `process_exit_code`. Adopt separate contract/physical-evidence claims and finite argument validation. Reject sensor command framing and exploratory-success policy; `EVIDENCE_REQUIRED` is not a shared ecosystem verdict. |
| INA228 | `4de84f2564eb6382835ca9a72d8688377657dcb4` | Unchanged since prompt 44: `tools/run_i2c_hil.py::classify_step`, `run_step`, `main`; prompt-43 exact-version seam is `version_contract_errors`. Adopt complete framed evidence, exact identity checking and failed short writes. Reject sensor recovery and its `UNKNOWN` exit override as CAN policy. |
| SHT3x-main | `7d04cd1a94a01eb9c144fd95d4ec95cff3d22ec3` | Unchanged since prompt 44: `tools/run_sht3x_hil.py::validate_parsed`, `run_dry`, `verdict`, `verdict_exit_code`, `parse_args`. Adopt finite duration, precise version expectations and an explicit dry-run/non-pass distinction. Reject reconnect/recovery and firmware fields absent from the CAN schema. |
| PCA9555 | `8b461b2afe9910b5b15fe32cc049d52e3b498970` | Unchanged since prompt 44: `tools/run_i2c_hil.py::validate_args`, `run`, `final_verdict`, `exit_code_for_results`. Adopt no serial import/open during dry run and retained timing policy. Reject serial-open retries and permissive operator-review exit policy. |
| FieldCore-node | `c40ea8341edf8d665460b902cafefbe857a7f73f` | Unchanged since prompt 44: `src/devices/rs485_motor/Rs485MotorModule.cpp::applyRuntimeConfig`, `startMotor`; `src/rs485/Rs485Task.cpp::applyPendingConfigs`. Adopt application ownership, no motor retries and retirement before configuration mutation. Reject importing product types or treating RS485 integration as implemented CAN support. |

The unchanged campaign/sensor/FieldCore seams were not reread. RS's older
prompt-43 Console audit used `cd27b12389f568198b72cecb6606123ea4c089bc`, so that
seam could not simply be declared unchanged at the current HEAD. Read current
`scripts/bench_probe.py::Evidence`, `Command`, `Console._lines`, `_decode`,
`_read`, `drain_startup`, `_complete`, `begin`, `wait`, `command`, `identify`,
`arguments`, relevant parser tests, `scripts/verify.py::check_inventory` and
actual CMake registrations. The source diff also establishes that
`scripts/_bench_evidence.py` itself is unchanged from that older revision.

Current RS adds drive-group commands, independent group stop capacity, a larger
16384-byte wire record bound and checked single-setting helpers. Those are real
RS changes, not obligations to add group operations or copy its capacities into
CAN prompts 43/44. RS still keeps one bounded Console owner, correlated request
and admitted operation identities, an immutable deadline and retained terminals.
Its handshake checks MotorControl-RS/protocol 3/capacity 10; it has no immutable
firmware boot/session ID. Uptime regression and reset diagnostics are evidence
of possible epoch loss, not proof that old wire responses are retired. CAN's
distinct schema, capacity and explicit missing firmware epoch remain necessary.

The current RS `Evidence._write` checks a full text write and latches every
write/flush exception before further work. This source contract exposed the
concrete CAN evidence finding below. RS private evidence remains supplied-data
validation; its `write_rejected` Modbus exception rule and
`home_inputs_permit` ESS input-function numbers are not transferable CAN rules.

## Current implementation and acceptance coverage

The six-file family has actual responsibilities, not filename placeholders:

| Original obligation | Current implementation and meaningful coverage |
| --- | --- |
| One Console, strict framing/handshake/correlation; pure evidence | `scripts/bench_probe.py::Console`, `Command`, `identify`, `read_fields`; private `_bench_evidence.py::summarize_operation`, `summarize_execution`; `console_jsonl.py` shares record validation. Console tests cover split UTF-8/lines, duplicate keys, malformed types, wrong protocol/version/profile, reused/wrong IDs, stale/buffered replies, deadline equality, frozen clocks, input/record limits, boot/disconnect and no replay. Pure evidence tests forbid clock/file/serial/session access and preserve exact decimal strings, native aborts, unknown execution and owned copies. |
| Finite explicit probe/read and offline modes | `bench_probe.py::parser`, `invocation`, `run_live`, `_self_test`. `bench_probe_cli_test.py` runs isolated subprocesses with serial imports forbidden, exercises all five read plans and explicit live argument/range failures before files or serial. `bench_console_native_test.py` consumes actual C++ producer output for probe, nine-field identity, all three state blocks and Q20 configuration refusal. State acquisition deliberately fails with retained transport uncertainty in that native demonstration; it is not counted as native successful state readback. |
| Named finite scenarios selected by prerequisites | `bench_scenarios.py::Scenario`, `build_scenario`, `validate_fixture`; `bench_motion.py` delegates the same candidate/session path. Plans include quick, position, stop, velocity, homing and settings. `bench_campaign_plans_test.py` checks exact command plans, ranges/crossed options, declared capability/method/stop prerequisites and no implicit enable/recovery/save. `bench_session_test.py::test_all_six_named_campaigns_use_actual_parser_and_session` supplies independent byte records through the real Console for every class; coverage is not limited to a mocked `checked` helper. |
| Bounded repetition with one planned cleanup | `bench_repeat.py::PositionComparison`, `build_scenario`: 1..20 write/verify pairs, exact aggregate displacement, ordinary relative moves and one final stop. Tests check bounds, exact integers above binary64 precision, unique operation IDs, early failure, one stop and no settings/save/restore/replay. `verify` can write differences; stored-only policy is unavailable. |
| One evidence-owning session, no replay, retained primary failure | `bench_session.py::run_recorded`, `_reserve_artifacts`, `package_report`; scenario cleanup stays in `Scenario.run`. Session/adversarial tests exercise serial-open failure, all artifact collisions, interruption, close failure, package/evidence failure, uncertain movement, failed/lost/late cleanup and primary failure retention. The first active upload is the DEC-1 probe; later active reads in the fake session are identity only. Q20-gated configuration is not used as a startup device read. |
| Usable roles, optional serial and report packaging | All five executable entry points have import/help/plan tests without serial; only `bench_scenarios` accepts `--list`, quick is read-only, and repeat accepts no arbitrary scenario. Package tests compare identical ZIP bytes, fixed metadata, an allowlist of three entries and SHA256/byte-count manifest content. Port/evidence failures remain nonzero; planned/host-tested/live-observed and hardware-qualified claims remain separate. |

This review found no missing scenario-class success test, undocumented working
runner role, unregistered runner suite or invalid documented offline command.
The concrete evidence-loss finding below was a requirement failure despite
the original registered suite passing; its correction and focused recheck are
recorded below.

## Confirmed evidence-loss finding

At the starting revision, `scripts/bench_probe.py::EvidenceLog.__call__` writes
JSONL with `self.file.write(line)` and ignores its returned character count.
Its normal write/flush exceptions also do not latch `exhausted`; only a capacity
limit does. Console already fences itself when its callback raises, but a short
write returning normally produces no callback failure. Work can continue after
the retained transcript silently loses bytes.

An independent host-only reproducer supplies this stream:

```python
class ShortSink:
    def write(self, line): return 0
    def flush(self): pass

log = EvidenceLog(file=ShortSink())
log("intent", command="probe 1000")
```

The observed result is `events=1`, `bytes=42`, `exhausted=False`, with no error.
No evidence bytes were retained. Existing tests inject raised callback/storage
errors or capacity exhaustion; they do not cover this returned short write.
This contradicts prompt 43's retained evidence and prompt 44's evidence-write
failure/no-further-work requirements, as well as the authoritative RS
`Evidence._write` contract.

The lead corrected `EvidenceLog` with an exact integer/full-length write check,
flush before counting a complete record, and a persistent `failed` latch for
serialization/write/flush failures. Capacity notices use the same checked write
path. This worker added four focused `EvidenceSinkAdversarial` tests in the
existing campaign adversarial suite: short/invalid return counts, flush error or
interruption, a recovered sink remaining fenced, short intent evidence preventing
command handoff, and short received evidence preserving unknown execution without
cleanup/replay. The new sink tests failed against the original implementation;
after the production correction all four and the registered complete campaign
adversarial suite passed. Production ownership remained with the lead.

## Independent review of the integrated corrections

Reread the actual combined production and test diff after the lead integrated
the independent workers' findings. These corrections close existing prompt
43/44 obligations; they introduce no additional motion route, retry, device
default or qualification claim.

| Confirmed defect at entry | Current correction and direct regression evidence |
| --- | --- |
| A checked `result ID` pending reply lost its admission proof in the pure summary. | `_bench_evidence.py::summarize_operation` now recognizes the correlated pending lookup, retains its admitted operation ID and reports `pending` without a terminal or invented native uncertainty. `bench_console_adversarial_test.py::test_pending_result_summary_preserves_confirmed_admission` checks both a client-retained original and a pre-existing firmware operation. The lookup request ID stays distinct from the original `command_id`. |
| An explicitly requested release refusal could be ignored after a known terminal. | `Console.wait(release=True)` and `run_live` check release `ok` and fail the requested invocation when release is refused. `test_wait_with_explicit_release_surfaces_refusal_and_retains_terminal` and `InjectedLive.test_refused_release_fails_invocation_without_erasing_read_terminal` retain original success/rejection, abort, operation identity and firmware result ownership; the release is attempted once. |
| Fixed read/startup turn counts could terminate valid supplied deadlines after roughly eight seconds. | `_turn_limit` derives a finite idle-turn budget from the requested duration, with two spare turns and a 65536 cap, for all commands, startup and pre-handoff processing. Long-read and long-startup adversarial tests cross ten supplied seconds successfully; frozen clocks still terminate under the explicit finite bound and fence later I/O. Neither command deadline nor native timeout is renewed. |
| Keyboard interruption in the one-invocation live wrapper could escape without its summary. | `run_live` captures interruption, recovers the admitted handle where present, records an interruption without replacing native evidence and returns nonzero. `InjectedLive.test_keyboard_interrupt_retains_admitted_unknown_outcome` checks interruption during transport read and an idle wait, preserved admission/unknown completion and no retry or cleanup. |
| Port-close failure could skip log close or leave a successful read invocation appearing successful; log-close failure also lacked retained separation. | `run_live` closes each independently before final summary publication, retains per-resource `close_errors`, preserves native terminal evidence and fails the invocation. `InjectedLive.test_close_failure_is_retained_separately_from_successful_read` checks both resources and asserts that failure in one close does not skip the other close path. |
| Short/failed evidence writes could silently lose transcript bytes or become reusable. | `EvidenceLog._write` checks exact integer/full length and flush before counting; `__call__` permanently latches serialization/write/flush failures, including capacity notices. The four `EvidenceSinkAdversarial` regressions cover invalid counts, flush errors/interruption, recovered-medium fencing, pre-handoff refusal and unknown execution without cleanup after received-evidence loss. |
| Preflight/intent evidence could consume the candidate deadline before the serial handoff. | `Console.begin` checks the supplied clock again after intent publication and before ID consumption, pending admission or `port.write`. `test_deadline_reached_during_preflight_never_hands_off_new_request` checks both a slow intent callback and elapsed empty input read; neither sends the candidate command. |
| Failed summary close/reopen could prevent invalidating an independently writable ZIP containing a premature pass. | `bench_session.py::_reserve_artifacts` attempts archive invalidation independently before summary repair. `bench_session_test.py::test_summary_close_and_reopen_failure_still_invalidates_passing_archive` makes both summary operations fail, retains the original close exception and checks that any readable archive cannot retain a passing summary. |
| A live invocation's summary write/flush/close failure could replace an available timeout diagnosis with a secondary setup error, or print completion before summary close failed. | `run_live` now completes port/log closure first, finalizes the summary within a checked write/flush/context-close boundary, and prints completion only after that boundary succeeds. On finalization failure it retains the native classification and primary reason in bounded stderr, reports the summary artifact error separately, returns execution exit 1 and performs no reopen, replay or cleanup traffic. Summary reservation remains a setup failure before port access. `InjectedLive.test_summary_finalization_failure_preserves_primary_diagnosis` covers six success/timeout by write/flush/close vectors: no premature completion output, bounded diagnostic, original timeout/classification and unchanged native evidence when the summary bytes remain readable. |
| A completed retained result could keep consuming its old service/input quotas while a later read waited, poisoning an otherwise valid long read. | `Console.pump` marks completed results `retained` only after an empty input read and empty partial-line buffer. `_read` and `pump` then freeze those results' per-command byte/turn counters; `wait` on an already drained retained result is a cached access without stream I/O. A completed result with an incomplete trailing record still consumes its original bounded budget. Three actual-flow adversarial regressions check a successful retained first result alongside a ten-second second read, frozen-clock trailing-line termination, and cached retained wait without I/O. Global session bytes, command count, request/operation slots, duplicate detection and original native evidence remain enforced. The empty host-input boundary establishes no firmware epoch or wire-retirement proof. |

The combined eight registered runner suites were rerun after these corrections:
8/8 passed in `.build/audit4344-coverage` (8.15 seconds). The additional summary
finalization and retained-result budget corrections were then read directly;
the summary regression's six failure vectors and all three retention regressions
passed independently. No further production defect was confirmed in the bounded
integrated diff. The lead owns the full verifier, affected
documentation synchronization, final commit and exact-commit remote CI evidence;
the current [integrated audit report](../reports/prompts_43_44_audit.md) records
that final handoff separately.

## Actual verifier and consumer boundary

`test/CMakeLists.txt` explicitly registers eight runner suites:
`bench_console_native`, `bench_console_contract`, `bench_console_adversarial`,
`bench_probe_cli`, `bench_evidence`, `bench_campaign_plans`,
`bench_campaign_adversarial` and `bench_session`, with 30/45-second timeouts.
`scripts/check_repository.py::test_sources`, registration checks and
`inventory_checks` require one real Python registration per test source and the
actual built `native_probe` argument for `bench_console_native_test.py`.
`scripts/verify.py` configures/builds, records actual CTest JSON, validates those
commands and rejects disabled, skipped or empty tests. Quick omits package/docs;
full adds isolated package consumers and strict pinned Doxygen 1.13.2 extraction.
CI uses Python 3.12, Windows quick GCC and Linux GCC13/Clang18 full sanitizer
gates. Firmware/SDK gates are distinct compile evidence and remain separate
from serial execution or motion qualification.

The source native console links the actual shared `MotorControlCANProbeApp`;
the Python native test consumes its `--console-fake` records, including retained
lookup/release. `test/package_test.py` copies the shared application/console and
native inputs into an isolated consumer using the relocated installed core. Its
generated CMake runs `native_probe --fake` and a correlated native console script
with an exact `9007199254740993` preparation. It does not run the Python runner
against that installed-consumer binary; no such passing check is claimed here.
Installed core remains independent of Python, serial, frameworks and vendor
reference distribution. No new package-public API is introduced by this audit.

## Executed host checks and limits

With Python 3.12.10 and GCC 15.1.0:

```text
cmake -S . -B .build/audit4344-coverage -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .build/audit4344-coverage --target native_probe --parallel 2
ctest --test-dir .build/audit4344-coverage -R '^(bench_console_native|bench_console_contract|bench_console_adversarial|bench_probe_cli|bench_evidence|bench_campaign_plans|bench_campaign_adversarial|bench_session)$' --output-on-failure
8/8 passed
The same eight registered suites passed again after the first eight corrections.
python test/bench_console_test.py InjectedLive.test_summary_finalization_failure_preserves_primary_diagnosis
1/1 passed (six success/timeout by summary write/flush/close vectors)
python test/bench_console_adversarial_test.py AdversarialConsoleTest.test_retained_terminal_does_not_spend_another_reads_service_budget AdversarialConsoleTest.test_terminal_with_partial_trailing_line_stays_bounded_with_frozen_clock AdversarialConsoleTest.test_wait_on_already_retained_terminal_is_a_cached_read_without_io
3/3 passed
python test/bench_campaign_adversarial_test.py EvidenceSinkAdversarial
4/4 passed after the evidence-sink correction
ctest --test-dir .build/audit4344-coverage -R '^bench_campaign_adversarial$' --output-on-failure
1/1 passed after the evidence-sink correction
python scripts/check_promptset.py
passed (planning integrity only)
git diff --check -- docs/audit/PROMPTS_43_44_REAUDIT_SOURCES.md test/bench_campaign_adversarial_test.py
passed
```

All 16 documented offline commands in `docs/runners.md` and
`docs/bench_scenarios.md` passed under a 15-second subprocess limit. Their exact
commands/locations/results are disposable evidence in
`.build/audit4344-coverage/documented_offline_commands.json`; CTest retained its
normal `Testing/Temporary/LastTest.log`. The short-sink reproducer above also ran
without any port or hardware dependency and exposed the evidence-loss defect.

These are checks of the reviewed worktree and the focused evidence-sink
correction, not verification of a later final commit. This worker did not run full verification,
isolated package builds, Doxygen, firmware compilation, remote CI or hardware.
No new device constant, firmware identity, physical completion, stop timing or
qualification claim is established. The existing Q20/native prerequisite,
missing firmware epoch, reverse velocity and hardware qualification limits
remain explicit. Final combined checks and commit/synchronization verification
belong to the lead.
