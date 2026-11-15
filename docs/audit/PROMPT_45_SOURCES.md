# Prompt 45 source audit

2026-11-15. Target entry: clean `main` at
`818d36186a558e650e3b39d92dbec731a9d6d487`, synchronized with `origin/main`.
All sibling inspection was read-only. No fetch, build-output write or source
change was made inside a sibling. Clean revisions below were checked at intake;
the handoff rechecks them. An unchanged revision does not excuse a newly required seam.

| Source and revision | Actual seam and disposition |
| --- | --- |
| MotorControl-RS `13cf955eeee278db9c01fd7acff1bedb0e516fb5`, clean | `include/MotorControlRS/Axis.h`, `VelocityOperation.h`, `src/profiles/ess_rs/Position.cpp::advanceMove`, `test/velocity_test.cpp::testSharedNumericPreparation`: adopt exact common intent, atomic preparation and retained context evidence. Explicitly set relative/unit/rounding/error policy; reject copying RS relative/RPM defaults, int16 RPM bounds or zero policy into CAN pure arithmetic. The 12 retained position vectors and nine source blobs were recompiled against this worktree and match. |
| Same RS revision | `examples/common/RtuBusOwner.cpp::admit`, `result`, `release`, `cancelActive`; `RtuRunner.cpp::cancel`, `RtuRunner.h::clearStats`, `test/runner_test.cpp::testProbeAndOwnership`: adopt copied requests, reserved urgent/results capacity, checked IDs and retained settlement. Reject importing RTU closure/quiet-time semantics as CAN channel reopening or processing-deadline rules. `examples/probe_cli/OwnerWatchdog.h::completed` separates loop supervision from drive execution; reset confirms nothing and replays nothing. |
| Same RS revision | Runner failure seam unchanged since prompt44: `scripts/bench_session.py::run_recorded`, `snapshot`, `cached_failure`; `bench_motion.py::phase_plan/run_phase/_run_phase`; `bench_scenarios.py::run_scenario`. Reuse the exact [prompt44 audit](PROMPT_44_SOURCES.md), with unchanged clean revision. Adopt finite public operations, scenario-owned cleanup and independent primary failure. Reject automatic CAN recovery, implicit motion/default fixture and ESS restoration assumptions. |
| ADS1115 `e07622a99fe9f13c5d6a5a2497a8112a14185b9d`, clean | `test/test_basic.cpp::test_owner_safe_tokened_read_publishes_atomic_sample_exactly_once`, `test_owner_safe_cancel_after_confirmed_start_waits_bus_silently_and_blocks_restart`: adopt matched owned results and bounded passive waiting; reject conversion quiet-time as CAN requalification. `test_owner_safe_pure_helpers_cover_boundaries_and_exact_units` zeroes invalid conversion output: do not copy that exception into CAN's atomic-output contract. Revision unchanged since44, but this owner/parser seam was freshly read. |
| INA228 `4de84f2564eb6382835ca9a72d8688377657dcb4`, clean | `test/test_basic.cpp::test_identity_parser_splits_die_and_revision_strictly`, `test_cooperative_initialize_budget_order_forwarding_and_alert_determinism`, `test_passive_health_never_suppresses_owner_requested_transport`: adopt strict identity parsing, bounded work and passive health separate from explicit transport. Reject I2C startup writes and sensor retry/readiness policies. Revision unchanged since44; newly required seam inspected. |
| FieldCore-node `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean | `include/TunnelMonitor/contracts/MotorControl.h::MotorResult`, `MotorCommand`, `sameMotorCommand`; `src/devices/rs485_motor/Rs485MotorModule.cpp` terminal projection and `takeMotorResult`. Keep immutable request/target, independent admission/wire/execution/setup/trigger/completion and historical uncertainty; taking a terminal does not stop, recover or reopen motion. Do not import RS addresses, ESS fields, application structs or its readiness policy. `config/build_profiles.json` still pins RS `v1.0.0`; this is not CAN integration. Prior configuration/ownership seam is unchanged since44. |

## Target and source origins

Read the actual Verify workflow, `scripts/verify.py`, strict native test
registration, package consumer and `scripts/check_stack.py`. Reused their pinned
tools and gates. No test-registration, package inventory or generated schema
changed: new cases extend registered suites and installed declarations retain
their shape.

Reopened pinned `docs/reference/public_code/srt/SRT_OpenCan.cpp` at source-pack
revision `30ec0a617fdbbcb6d17b42edc85c5210344f6796`: global status cache, upload
initiation, write widths, enable/move/stop, running timeout, homing and zero/save.
These are public-code defects, not CL86-C response evidence. EN86's `6061`
instruction is a separate manual conflict; `605A/6085` assumptions are generic
CiA402 assumptions, not SRT facts. The [report](../reports/prompt_45.md) maps each
to literal host regressions. No vendor constant, timing, status dialect, original
PDF, EDS, generated dictionary or physical qualification was changed here.

The three production corrections concern host ownership/evidence only. Device
commands are unchanged. Any manufactured state, PDO retirement boundary or fake
reply in the tests remains `HOST_FAKE`, not a measured capability.

## Concurrent sibling change at final review

RS kept the same HEAD but became dirty after intake: `docs/ess_pair_writes.md`,
four `docs/prompts/standalone` files (02/04/05/11), new standalone15/16/17 prompts,
`examples/probe_cli/PositionStageApp.h`, `ProbeApp.h`, and `test/group_app_test.cpp`.
The inspected application diff copies the borrowed outcome before owner release
and prevents cancelled staging from publishing a stored-word witness after
readback. These are **uncommitted sibling changes**, not behavior attributed to
HEAD. The audited core/common/owner/runner files and position-vector source closure
are unchanged; CAN already retains terminal evidence before release and separately
invalidates prepared witnesses. No sibling work was modified or included in this
commit. ADS1115, INA228 and FieldCore remained clean at their recorded revisions.
