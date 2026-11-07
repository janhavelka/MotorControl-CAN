# Prompt 34: wrapped-angle motion integration

## Scope and status

Prompt 34's software scope is complete: common wrapped-angle preparation feeds
the existing finite position context, owner and uncertainty path. The consumed
prerequisite is prompt 33's checked `Position.h/.cpp` lifecycle plus handoff
packets 20/21 and bench-continuation packet 2's `validateMoveAdmission` contract.
Their software artifacts and tests exist; their Partial stage status does not
turn unknown physical scale or firmware behavior into established facts.

Inherited gates remain Q04/Q14 and the applicable prompt-33 firmware/reference
evidence. No physical angle campaign, new profile, CLI, motion executor or
unqualified default scale is added. This prompt's scope explicitly keeps
physical qualification separate. No port was opened or firmware flashed.

## Source refresh and evidence

Entry: `1abb6aa` on `promptset/motorcontrol-can`, tracking
`origin/promptset/motorcontrol-can`. Fresh fetch showed 0 ahead/0 behind, no
staged changes, and three unrelated untracked `.vscode` JSON files. They are
preserved and excluded from this change. This is the existing promptset-v2
execution, not a new branch or worktree.

| Source | Revision / state | Behavior adopted or rejected |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | `include/MotorControlRS/Units.h:113` and `test/position_interrupt_test.cpp::rejectedAdmission` establish unchanged output on preparation failure. `src/profiles/ess_rs/Position.cpp::prepareMoveAngle` selects wrapped absolute intent and the existing move engine; `test/position_test.cpp::testAbsoluteAndAngleReuseSequenceAndRetainReference` preserves witness990 -> target1000. Current `examples/probe_cli/ProbeApp.cpp` dispatches the same request by wrapped/relative. Adopt those meanings; reject ESS wire registers, native widths, boot scale and preparation-only freshness as CAN authority. Units/Axis/Position implementation bytes are unchanged from the preceding RS comparison; changed callers/tests were reopened. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Identity/admission seam unchanged since prompt 33 and handoff packet 20: `MotorControl.h` and `Rs485MotorModule::startMotor` retain configuration epochs plus separate operation/step identities. No reread of that unchanged seam; no angle/velocity operation or sibling edit is claimed. |

Prompt 34 names no I2C sibling: output-on-failure authority is the actual RS
contract and CAN's own tests. No unrelated sibling suite ran.

Primary originals reopened: EN86 and ZH86 PDF pp11/12/18, OP86 pp9/10, and EDS
sections `2200`, `2201`, `607A`, `6064`, `6081`, `6083`, `6084`. SHA-256 values:

- EN86: `401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f`
- ZH86: `3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13`
- OP86: `4c87679d210c99f4ac4f6ce29e7f2c82b13221fdf81c47ee59a989f708f504b5`

The source-confirmed target is I32 +/-1,000,000 pulses, speed U32 rpm and ramps
U32 ms. `2201`'s default0 and contradictory documented ranges do not prove
increments/turn. `2200` is U16 with a direction name; its Pulse Sign/Pulse /Sign
table proves neither shaft polarity nor its CANopen effect. These claims and
Q04 stay unchanged. Existing node-1 shaft observations remain specimen evidence,
not an assumption of 4000 counts/turn from encoder lines. This prompt adds no
new physically measured or inferred device behavior.

C1 rechecked the original Nanotec N5 manual v3.3.0/FIR-v2213, pp38/62-64,
including rendered p64. The 3600 vector applies at gear1:1; degrees include
`6091`. Original SHA-256:
`4c60e1415c3f2421a010c57d3fd050b49bed87606a0fda769dd874052867ea41`.
The official EPOS4 rel13740 PDF pp221/234 was reread; current bytes match the
recorded hash `66fe32f15ab0bcbf9a008d0d0da8ffe395064c6716edcb3f3b28907f7160e64f`.
Selected main-sensor feedback and increments-only SI position do not supply a
factory servo scale. The 8192 command / 2048 feedback, 3:2 and polarity-1
vectors are explicitly abstract common invariants. No original was changed.

## Implementation and decisions

- `prepareMoveAngle` / `validateMoveAngle` in existing `Position.h/.cpp` select
  wrapped absolute intent in the shared preparation. `preparePositionParameters`
  now accepts that intent through production `preparePosition`. No conversion is
  repeated or round-tripped through floating point.
- The same `MoveContext`, `nextMove`, `advanceMove`, setup/`001F` trigger,
  acceptance/completion observations, rearm and unknown-effect settlement run.
  `AxisSession::requestMoveAngle` shares its private request path and all owner
  fences with absolute/relative moves. The absolute/relative entry points still
  refuse the wrong request form.
- Angle admission needs an explicit host ACTUAL-basis policy, fresh idle and
  stationary multi-turn witness with age cap, matching epochs and mechanical
  origin. Common pure math remains broader; a `005F` wire-base claim is not
  needed for the absolute angle endpoint.
- Original request/path/tie/frame, exact endpoint, quantization/error budget,
  configuration and reference are retained. Exact paths/turns are recoverable;
  `PreparedTarget::nativeDirectionPositive` preserves the native sign for
  explicitly approximate radians. It is meaningless at zero, and not a physical
  direction label under negative polarity.
- Chosen-path limit failure never switches turn. Zero effective displacement
  and BINARY32 targets fail before admission. Parameter writes retire retained
  MOTION_PARAMETERS observations without advancing their own configuration epoch.
- The caller retains typed DRIVER `directionRaw`/`subdivisionRaw` under the
  current epoch. A contradictory raw2200/2201 baseline explicitly invalidates
  coordinates even if software polarity did not change. No automatic discovery,
  read, reset, enable or protective-input reassignment occurs.
- Independent review found `validateMoveAdmission` could succeed exactly at its
  cutoff although next/transmission already refused. It now rejects
  `now >= deadlineUs`; a fresh-evidence regression isolates that condition.
- C1 keeps N5/servo fixtures test-only and removes the prior misuse of N5's
  documentary `3202` motor-kind values as `Endpoint.profile` IDs.

## Tests and consumers

`test/move_angle_test.cpp` exercises the actual finite owner with its bounded
fake node, both with and without interleaved heartbeat traffic. It covers
positive/negative wraps, multiple turns, exact half-turn policies, polarity,
motor/load origins/gearing, chosen-path limits, fractional quantization and
zero, missing/future/stale references, changed mode/binding/config/reference,
raw-direction invalidation, unchanged output/queued work, reference rechecks,
own-parameter observation retirement, urgent stop, unknown trigger/late ACK and
processing-time completion/admission cutoffs. A finite angle of
`9007199254740993` degrees resolves to33 at scale360 (negative to-33), which
would change through binary64. Huge native endpoints remain exact in common
math and correctly fail the finite I32 profile range.

`examples/angle_preview` supplies an illustrative command scale, gearing,
origin, limits, witness and rounding. It displays effective239 and exact error
+1/9, then shows the default closed move gate with no owner admission or CAN.
It is registered both in native CTest and the relocated-installed consumer.
No new installed header/source was needed; build/test/package inventories were
updated for the new native consumer and test. Strict existing headers compile
independently with declared dependencies.

Focused commands already passed:

```text
cmake -S . -B .build/p34-native -G Ninja -DBUILD_TESTING=ON
cmake --build .build/p34-native --target move_angle_test move_absolute_test move_relative_test move_commanded_test axis_test position_parameters_test multidrive_contract_test angle_preview --parallel 4
ctest --test-dir .build/p34-native -R "^(move_angle|move_absolute|move_relative|move_commanded|axis|position_parameters|multidrive_contract|angle_preview)$" --output-on-failure
doxygen Doxyfile
```

Focused 8/8 passed; angle suite reports 26,472 checks. Final integrated checks:

| Check | Result / retained evidence |
| --- | --- |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p34-full` | Passed: 101 registered native/Python/documentation contracts, repository and inventory checks, version/object freshness, independent public headers, source/export/relocated-install consumers including angle_preview, C++11/C++17/no-RTTI. Summary and exact command/tool logs: `.build/p34-full/summary.json`. |
| Strict Doxygen1.13.2 and extracted public API check | Passed: zero warnings, 28 headers, 2535 members, 103 free functions. Output/log retained by full verification. |
| `python scripts/rs_move_vectors.py --rs ../MotorControl-RS --revision cd27b12389f568198b72cecb6606123ea4c089bc --keep ABSOLUTE_BUILD_PATH` | Passed: 12 cases and nine current RS source blobs match retained independent vectors. `ABSOLUTE_BUILD_PATH` was the absolute checkout path to `.build/p34-rs-comparison-absolute`. An initial relative `--keep` invocation misclassified dependency paths and reported only path differences; the absolute invocation resolved that audit-tool invocation issue without changing source or expected vectors. |
| S2 actual compile/link | Passed, ESP-IDF5.5.5 and Xtensa14.2.0: `.build/p34-s2-firmware/summary.json`, 506 compilation entries, actual shared owner/application/core inputs, ELF/BIN and logs retained. No flashing. |
| Strict S2 stack check, actual compile database, both `-Os` and `-Og` | Passed: zero frame/chain breaches, empty baseline. `.build/p34-stack/report.json`; `prepareMoveAngle` frames976/992 bytes, MoveContext3008, AxisSession28784. Call-chain bounds exclude unknown/indirect/SDK frames and are not a runtime stack high-water measurement. |
| Promptset integrity, generated object check and `git diff --check` | Passed: 140 retained artifacts, 99 EDS indexes, 202 typed entries and 53 manual items remain accounted for. Original bytes and attributes unchanged. INDEX and post-commit HEAD checks are part of synchronization. |

The S2 build used the existing reviewed environment, without changing an SDK or
sibling repository:

```powershell
$env:IDF_PATH = 'C:/pio/packages/framework-espidf'
$env:IDF_TOOLS_PATH = (Resolve-Path ../MotorControl-RS/build/p25/idf-tools).Path
$env:ESP_ROM_ELF_DIR = 'C:/pio/packages/tool-esp-rom-elfs'
$env:Path = 'C:/pio/packages/toolchain-xtensa-esp-elf/bin;C:/pio/packages/tool-cmake/bin;C:/pio/packages/tool-ninja;' + $env:Path
python scripts/check_firmware.py --target s2 --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p34-s2-firmware
$firmwareResult = Get-Content .build/p34-s2-firmware/summary.json -Raw | ConvertFrom-Json
$compileDb = Join-Path $firmwareResult.sdk_build_evidence.s2 'compile_commands.json'
python scripts/check_stack.py --require-toolchain --toolchain-dir C:/pio/packages/toolchain-xtensa-esp-elf/bin --build-dir .build/p34-stack --report .build/p34-stack/report.json --strict --target esp32s2 --compile-commands "esp32s2=$compileDb" --jobs 2
```

Host tools: GCC15.1.0, Python3.12.10, CMake/CTest4.0.1, Ninja1.12.1,
Doxygen1.13.2. Local Clang/Linux sanitizer gates are unavailable on this Windows
host; the existing automatic CI runs GCC/Clang ASan/UBSan, Windows quick,
Arduino and IDF compile/component/stack jobs on the exact final commit. Its
terminal result and link are recorded in the handoff/receipt, not inferred from
an older green run. No required owned local check remains unperformed.

## Documentation, review and handoff

Public Doxygen and the position/axis guides document units, path/tie/frame,
reference lifetime, native sign, generation dependencies and failure semantics.
README, CHANGELOG, source map, API/CLI map, coverage, backlog and roadmap are
updated. The single ledger's position implementation-scope note is corrected
and regenerated; no source claim, access permission or qualification flag changes.

Delegation: `p34_angle_review` (gpt-6-astra/high) independently checked RS,
vendor contrasts, path/limit/reference semantics and the final production diff;
`p34_contrasts` (gpt-6.1-sol/high) owned only C1 tests;
`p34_tests` (gpt-6.1-sol/high) owned only the angle test/example. The lead owned
public contract decisions, integration, docs, verification and Git. No worker
performed hardware or Git mutations. Integrated results are the lead's checks.

Maintainability: reused the exact conversion, finite phases, retained contexts,
owner queues and stop/uncertainty path. Added no module, runtime registry,
executor, controller receiver, allocation or capacity. Only private form
selection and one retained native-direction boolean were needed. No cleanup is
deferred. Original physical/firmware gates stay with their assigned owners.

Next numbered scope is prompt35; independent CLI/offline work remains as
recorded in PROGRESS. Only prompt34 is assigned here. Final commit/sync and exact
CI receipt will be recorded in the user handoff and ignored `.build/p34-ci/`.

## Independent follow-up audit

The [fresh prompts 34/35 audit](prompts_34_35_audit.md) rechecks the original
requirements against code and tests, records the velocity poll-budget correction
and final verification, and keeps firmware/physical gates separate.
