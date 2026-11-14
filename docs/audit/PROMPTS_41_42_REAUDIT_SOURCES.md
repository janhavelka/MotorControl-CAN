# Prompts 41 and 42 source, coverage and platform re-audit

Checked 2026-11-15 against `main` at
`635cb95e7816fb57073212bd584e944799b0f009`. This is an independent,
bounded review of the original prompt 41/42 obligations. It does not treat the
earlier source audits or stage verdicts as proof of the current implementation.
No sibling files, Git state, SDK build, device or hardware were changed.
The lead owns integration and execution of the actual verification gates.

## Current sibling sources and delta

| Repository | Refreshed revision and worktree | Actual source reviewed; disposition |
| --- | --- | --- |
| MotorControl-RS | `13cf955eeee278db9c01fd7acff1bedb0e516fb5`, clean | `examples/probe_cli/ProbeConsole.cpp::COMMANDS`, motion/action dispatch and capability dispatch; `ProbeApp.cpp::startTypedRead`, `startDriver`, `startActionIn`, `startMove`, `lookup`, `cancel`, `release`, `selectTarget`; named motion-profile/persistence deltas. Adopt explicit requests, retained command/operation identity, pre-admission checks, and cancellation that settles accepted traffic. Reject serial endpoints/registers, boot geometry/speed/origin, unrelated firmware evidence and implicit replay. |
| FieldCore-node | `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean | Read `include/TunnelMonitor/contracts/MotorControl.h:110-154`, `Rs485MotorModule.cpp::applyRuntimeConfig`, and the task ownership seam. Those three source files have no diff from the prompt 41/42 recorded revision `5ad9292dc8481cf08be9a61f662495a6474d1da0`: the named seam is unchanged since prompt 42 despite the changed repository HEAD. Adopt independent admission/wire/execution, setup/trigger effects and historical uncertainty; reject importing FieldCore product types or treating a result take as reconciliation. |

The RS revision recorded by both older audits was
`cd27b12389f568198b72cecb6606123ea4c089bc`. It is no longer current. Actual
diffs in the named seam add independent drive-group pools/reservations,
position staging, single-setting/start-speed routes, group-aware exclusion and
fixture-only broadcast grammar. `MotionProfileApp.h::serviceMotionProfile` now
uses the configuration of its own target, and persistence/motion-profile work
also excludes group sequencing. These are real deltas, so an "unchanged since
prompt 40" claim would be wrong for this fresh audit. They do not enlarge CAN
prompt 41/42 into group or broadcast implementation obligations.

Current RS `startActionIn` still permits stop replacement without checking
normal-to-fast direction, and uses RTU-specific settlement/recovery details.
CAN must retain its deliberately stronger escalation check and independently
retained stop evidence. Current RS `cancel` uses `cancelUnsent` and a
`cancelContinuation` flag; accepted work settles naturally. `release` requires
delivered terminal evidence. `selectTarget` invalidates target-dependent scale,
origin, qualifications and commissioning confirmations without sending traffic.
Those meanings remain applicable; their wire values and policy timeouts do not.

FieldCore's actual dependency policy still identifies RS v1.0.0, tag object
`58d74592a9fe9fa4bd31cbe5e449a51d30879721`, peeled commit
`4125a092d91b4b6423d1664ee51fff209110006f`. Its motor boundary is not an
integration with current RS HEAD or with CAN. `MotorResult` retains immutable
command identity, owner-observation time, wire settlement, execution, independent
setup/trigger effects and historical `executionUnknown`. `MotorStatus` separates
ordinary and stop reservations. `Rs485MotorModule::applyRuntimeConfig` rejects
address/baud changes, nonzero retry policy and changes while active/result-ready;
it is not a generic motor commissioning mechanism.

For the independent route review's discovery finding, the current RS
`examples/probe_cli/DiscoveryApp.h::discoveryCommand` stores
`scan.originalTarget = a.axis.target` and the original host tuple.
`serviceDiscovery` uses separate `discoveryPrepared` requests rather than
selecting each discovered address as the application axis; `discoveryRestore`
restores the original host tuple. Explicit `ProbeApp.cpp::selectTarget` owns
selection and its generation/qualification invalidation. Adopt that separation:
scan findings do not silently select a target. CAN's audited
`DiscoveryScan::advance` called `app_.select(t)` for candidates; the lead owns
the correction and its independent route regression.

## Selected original device facts reopened

Read directly from retained original PDFs using PyMuPDF, not the extracted
research notes. Page numbers below are physical PDF pages. No new constant or
device-facing behavior is introduced by this report. The manuals cover the
CL86-C/OL86-C family without a matched firmware revision; the EDS is a generic
family artifact. Documentary consistency does not qualify live firmware.

| Claim checked | Original evidence and current disposition |
| --- | --- |
| Position speed versus ramps | EN86/ZH86 PDF11, EDS `6081:00`, `6083:00`, `6084:00`: RW U32, speed 0..3000 rpm, ramp times 0..2000 ms. Milliseconds are not physical acceleration. EDS defaults are not permission to use boot settings. |
| Velocity destination and signedness | EN86/ZH86 PDF13 labels `60FF:00` 0..3000 rpm/U32, while EDS `[60FF]` is I32 (`0x0004`). Preserve this contradiction and exact firmware/reverse qualification; never infer negative encoding from the signed EDS alone. Position speed uses `6081`, not `60FF`. |
| Requested/observed mode | EDS `[6060]` is WO U8, `[6061]` RO U8; PDF11 distinguishes requested mode from monitored mode. Parser dispatch cannot write observed mode or choose an unresolved dialect. |
| Stops and release | ZH86 PDF12 contains the `15->271` stop and `15->11` emergency-stop examples. EN86 differs. These arrows do not authorize enabling before a stop, and a controlword/status correspondence does not establish a release command. Preserve D-17; `motor-release` is gated. |
| Counter clear versus save/origin | EN86/ZH86 PDF15 states `2302:00` 0-to-1 clear with manual return to zero, and separately `2300:00=2` save. EDS declares both U16 RW. A device clear never implies save, homed state or a host origin. |
| Stored node and factory restore | EN86/ZH86 PDF18: `2203:00` CAN ID 0..127; `2300:00=1` factory restore, `=2` save. This is not host selection, active-route proof or restart/durability timing. Zero is not an ordinary CANopen endpoint. |
| Diagnostic output status | ZH86 PDF18 maps `2102:00` bits0..2 to Y0..Y2 status. EDS calls it RW U16, preserving the manual/access contradiction. No output-command layout or terminal electrical level is inferred from this diagnostic. |

Recomputed SHA256:

```text
86C_CANopen_user_manual_en.pdf
401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f
86C_CANopen_user_manual_zh.pdf
3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13
LC_StepDriverCAN_V1.1.eds
dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c
```

These bytes match the earlier audits. Existing DO applicability, homing wiring,
Q20 upload effects, stop completion and qualification obligations are not newly
resolved here. No physical measurement was made.

## Canonical command coverage and actual tests

The one actual compiled inventory remains
`examples/common/ProbeConsole.cpp::commands`, exposed by `consoleCommands`.
It contains 57 rows at the audited revision: 51 working conditional route
families and six gated rows. The six gated rows are `settings`, `subdivision`,
`motor-release`, `motion stored`, `home qualification` and
`velocity qualification`. They are excluded from working help/dispatch and
are not implemented native capabilities. This count does not treat every
subcommand, external qualification, or surveyed profile as an implemented
feature. The table owns grammar/API/capability/effect/completion; this report
does not create a competing inventory.

`ProbeConsole::serviceListing` emits that inventory. `dispatch` rejects gated
rows before routing. `profile list` uses the real discovery-profile inventory
and reports only Lichuan C, with unknown model/firmware/motor/feedback evidence.
`docs/cli_contract.md` and `docs/api_cli_mapping.md` distinguish working
conditional routes from supplied prerequisites and gated native support.
Ordinary unaccepted native-group reads remain whole-plan Q20 rejections.

Actual registration in `test/CMakeLists.txt` and validation by
`scripts/check_repository.py` includes `probe_console`, `console_protocol`,
`probe_device_console`, `probe_settings_app`, `probe_commissioning_app`,
`motion_intent`, `motion_console`, and `probe_motion_actions`.
`console_protocol_test.py` consumes the actual console/native/motion binaries
and parses their JSONL output. Merely compiling these sources is not test
execution; this worker did not run the combined suite.

The reviewed tests include whole-plan read refusal, identity unsigned payloads,
passive discovery, stale selection/route epochs, exact attachment ownership,
explicit NMT scope, settings partial loss/readback failure, commissioning/save
lost replies, motion arithmetic and simple intent without defaults, retained
motion plus stop results, congestion/escalation, cancellation settlement,
device-zero without save, and replaced native-context identities.
`probe_motion_actions_test.cpp::finiteVelocity` exercises actual forward/reverse
console payloads and finite-stop timing; `homing` exercises method29 success
and lost-trigger uncertainty, with method17 qualification refusal. These must
be included when judging route coverage; reviewing only `motion_console_test`
would incorrectly report velocity/home success as absent.

Two confirmed coverage findings were sent to the lead:

1. At the audited revision, inventory metadata for `result`, `release`, and
   `cancel` still describes only read/recovery handling. Actual dispatch also
   supports settings, commissioning, discovery and motion. Update the API and
   capability/help strings to describe the completed correlated operation path;
   grammar and behavior need no change for this finding.
2. At the audited revision, the shared-console C++ tests had no actual
   `driver set RPM UP_MS DOWN_MS` or `control set 0|1|2` invocation. Direct application tests
   `driverRequiresActualControlAndCompleteCandidate` and
   `loopSelectorUsesTypedReadbackWithoutEncoderWrite` do not test the separate
   argument branches in `ProbeConsole::dispatchDevice`. Prompt41 requires each
   implemented group's route-to-public-API parity and legal/illegal
   pre-admission cases. This worker closed that gap with
   `consoleDriverCandidateUsesTypedUnitsAndReadback` and
   `consoleControlCandidatePreservesEncoderAndRejectsInvalidInput` in
   `test/probe_settings_app_test.cpp`: actual console success, typed native
   values/getters, retained result correlation and range/shape/fraction rejection
   before traffic or operation consumption. Python campaign strings and the
   separate bench console did not cover these shared parser branches.

The added tests pass in `.build/audit4142-coverage` using GCC15.1.0:

```text
cmake -S . -B .build/audit4142-coverage -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .build/audit4142-coverage --target probe_settings_app_test --parallel 2
ctest --test-dir .build/audit4142-coverage -R '^probe_settings_app$' --output-on-failure
1/1 passed
```

The invalid-candidate vectors all advance the console's rejection counter and
leave wire count, next operation ID and retained settings unchanged. Driver
success checks `6081` rpm plus `6083/6084` milliseconds; control success checks
only the loop selector changes and the encoder word remains unchanged. No
production implementation defect was exposed by these added vectors.

No additional code-complexity defect was confirmed in this bounded review.
The concrete `AxisSession`, `ProbeMotion`, settings and commissioning owners
remain distinct; the parser delegates native lifecycles rather than rebuilding
them. The lead must reconcile findings against the final combined tree.

## Actual platform and verifier boundary

Arduino and IDF consume `examples/common/firmware_sources.json` through
`scripts/pio_firmware.py` and `examples/probe_idf/main/CMakeLists.txt`.
Native `MotorControlCANProbeApp` compiles the same console/motion/settings/
commissioning sources in `examples/common/CMakeLists.txt`. The manifest includes
the actual `ProbeConsoleMotion.cpp`, `ProbeMotion.cpp`, `MotionIntent.cpp`,
`AxisSession.cpp`, settings, commissioning, owner and CANopenNode adapter inputs.
It does not substitute a firmware-only motion parser.

`EspProbeMain.cpp::ownerTask` attaches the shared device facility and services the
same console. Default native/firmware entries do not attach reviewed motion or
settings/commissioning sessions, configure live routes or start CAN. Therefore
compiled grammar parity is established by source selection; callable device
behavior still requires the explicit application graph/prerequisites. This is
documented in the CLI and firmware guides, and is not hardware motion support.

Actual CI in `.github/workflows/verify.yml` runs quick Windows GCC, full Linux
GCC13/Clang18 with ASan/UBSan, pinned Doxygen1.13.2, Arduino3.3.11 via
PlatformIO6.1.19, and ESP-IDF5.5.5 shared S3, S2, OPTAK smoke, separate S2 bench
and isolated component builds. SDK source hashes and tool versions are checked.
`scripts/check_firmware.py` verifies actual compilation databases, source-set
membership, selected target/configuration and linked ELF/BIN; a manifest alone
does not satisfy that gate. Stack checking uses real S3/S2 compile options.

The separate `idf-s2-optak-console` target consumes the shared UART0/single-core
console without the smoke hook and is explicitly outside `--target all`.
Arduino S2 is explicitly blocked by the pinned package's UART0 SDK-console
ownership conflict. `check_firmware.py --target arduino-s2` reports that blocker,
and neither CI nor documentation silently counts it as passing parity.
P4 core evidence is not a P4 shared-console image or adapter qualification.

`scripts/verify.py` checks repository/generated version/object freshness,
builds, verifies actual CTest registration and rejects skipped/empty tests.
Quick mode omits package/documentation; full mode additionally runs isolated
package consumers, Doxygen and public XML extraction. Firmware compilation,
full native checks and exact-final-commit remote checks remain the lead's
separate obligations. No new passing check or hardware qualification is claimed
for those gates by this source review; the focused settings test above is the
worker's executed software check.
