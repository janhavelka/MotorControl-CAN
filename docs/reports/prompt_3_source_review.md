# Prompt 3 current sibling semantic review

Read-only source review on 2026-10-28 for prompt 3. This supplements the lead's
semantic ledger review; it approves no Lichuan device fact. The existing branch
is `promptset/motorcontrol-can`. No Git mutations, sibling edits, hardware work
or sibling test runs were performed.

## Revision and dirty-state refresh

The following actual sibling paths were inspected before and after the source
reads. Every listed HEAD was unchanged and every worktree was clean at both
observations. Previously recorded revisions are unchanged; MCP45HVX1 and
RV3032-C7 add the action/persistence contrast needed here.

| Path relative to `Projects/` | HEAD |
| --- | --- |
| `MotorControl-RS` | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` |
| `ADS1115` | `235af3e8c722661a89a44766a4ccd903a654f6fe` |
| `INA228` | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` |
| `FieldCore-node` | `5ad9292dc8481cf08be9a61f662495a6474d1da0` |
| `MB85RC` | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` |
| `MCP45HVX1` | `c6c7feefd79b9fc16ddc7b7786ad187e5185fde6` |
| `RV3032-C7` | `aabd8d9204e75072403fefb60cf8d2679a7f56de` |

The substantive delta is the prompt-3 inspection seam, not changed sibling
behavior. RS profile declarations currently reside under
`include/MotorControlRS/profiles/ess_rs/`, with implementations under
`src/profiles/ess_rs/`; historical root-header paths must not be copied.

## Declarations, implementations, callers and tests

Paths in this section start at the named sibling root. Line numbers refer to
the revisions above.

| Source seam | Current evidence and implication for CAN review |
| --- | --- |
| RS native access and type | `include/MotorControlRS/profiles/ess_rs/Types.h:8` explicitly calls `RegisterAccess` source access, not transaction authorization; values are `READ_ONLY`, `WRITE_ONLY`, `READ_WRITE`, `UNSPECIFIED`, `RESERVED`. `:10` distinguishes `SIGNED_ENCODING_UNRESOLVED` from `UNSIGNED`, bitfields and enumerations; `:15` has independent `RegisterIssue` flags. `scripts/generate_ess_registers.py:170` makes compact access data separately from optional catalogue strings, excludes reserved rows and limits raw single writes to one-word source-writable rows. CAN should preserve raw EDS metadata separately from reviewed permission, effect, encoding and applicability; RS access generation is not proof of Lichuan CAN authorization. |
| RS typed observation and retained effect | `include/MotorControlRS/profiles/ess_rs/DriverSettings.h:101` defines `DriverObservation` with raw values, `knownFields`, configuration generation and per-read provenance; `:117` defines exact candidate-bound `DriverPrerequisites`; `:179` separates acknowledged/readback/active facts in `DriverProgress`; `:191` defines caller-owned `DriverContext` with retained `effects` and `uncertain`. `src/profiles/ess_rs/DriverSettings.cpp:703` publishes `getDriver` only after a complete successful READ and successful local decode; `:732` makes `getIo` a checked typed projection retaining unknown bits. `test/driver_settings_test.cpp:81` preserves unknown raw fields; `:92` checks unchanged failed-getter output; `:235` tests partial effect and readback mismatch; `:258` keeps an unknown exception uncertain. `examples/probe_cli/ProbeApp.cpp:1263`, `:1383`, `:1692` are actual prepare/readback consumers. CAN can reuse intent/names, not RS addresses, terminal counts or native units. Matching readback must not be promoted to known activation/persistence. |
| ADS1115 configuration trust | `include/ADS1115/ADS1115.h:24` separates `ConfigurationState` from driver health; `:95` defines typed `SampleResult`; `:121` defines `AppliedProfileSnapshot` whose generation counts verified commits, including unchanged settings. `:128` separates operation status from `hardwareStateUncertain`. `src/ADS1115.cpp:693` verifies low/high thresholds and CONFIG, entering UNKNOWN on mismatch. `test/test_basic.cpp:859` checks failed initialization plus preserved dirty diagnostics. `examples/02_owner_safe_poll/main.cpp:237`, `:248`, `:328` supplies time, polls with budget one and consumes the exact token. This supports explicit provenance/trust facts; its recovery/profile replay and wrapping generation semantics are not motor policy. |
| INA228 consuming reads | `include/INA228/INA228.h:110` documents destructive `DiagAlert` reads; `:133` distinguishes cache-only `DiagAlertSnapshot`; `:197` defines `JobEffect::{NONE,CONFIRMED,PARTIAL,INDETERMINATE}` independently of status. `src/INA228.cpp:2886` captures successful tracked reads and preserves sticky event evidence. `test/test_basic.cpp:2034` proves CNVRF/latched limit clearing while overflow flags remain. CLI consumers explicitly choose cached `getDiagAlertSnapshot` at `examples/01_basic_bringup_cli/main.cpp:921` or destructive `readDiagAlert` at `:987`. CAN's reviewed ledger must distinguish consuming/unknown read effects from access flags; an upload is not harmless merely because it is a read. |
| MB85RC model applicability | `include/MB85RC/CommandTable.h:11` defines explicit `DeviceVariant`; its `VariantInfo` records capacity, addressing and ID/Sleep/HS capabilities separately. `include/MB85RC/MB85RC.h:239` documents bus-silent explicit binding and AUTO's identity prerequisite. `src/MB85RC.cpp:2017` accepts an identified variant only after manufacturer/product checks and discards variant-dependent address/HS evidence on change. `test/test_basic.cpp:976` verifies AUTO's expected versus active variants; `examples/espidf_basic/main/main.cpp:594` and `:606` show actual explicit policy and identity read. CAN needs separate user-selected model provenance and generic EDS identity; MB85RC's discoverable IDs do not establish that CL/OL models are auto-identifiable. Its retained transfer buffers are not CAN codec buffer ownership. |
| MCP45HVX1 action versus value | `include/MCP45HVX1/CommandTable.h:39` defines `Command::{WriteData,Increment,Decrement,ReadData}`; WIPER permits INC/DEC while TCON is R/W only. `src/MCP45HVX1.cpp:1148` sends repeated step commands, updates a known cache only after success and invalidates certainty on an ambiguous failure. `test/test_basic.cpp:1668` applies one increment/decrement before failure and checks the unknown cache; readback reconciles the value without replay. `examples/espidf_basic/main/main.cpp:1754` routes INC/DEC explicitly. Therefore an action/edge command is not a replaceable setting, even at a writable address. CAN save/reset, clear and trigger semantics need their own owners and prerequisites. |
| RV3032 persistence uncertainty | `include/RV3032/RV3032.h:319` defines `ConfigurationFinalState`; `:334` defines `ConfigurationJobReport` with distinct operation/cleanup status and mutation-attempt evidence; `:342` and `:353` separate persistent-read proof and partial user-write proof. `src/RV3032.cpp:5486` verifies staged data before WRITE_ONE; `:5518` dispatches WRITE_ONE once and retains a failure while later read proof determines durability. `test/test_native/test_datetime.cpp:3323` proves ignored staging prevents persistence and an ambiguous committed command is attempted exactly once. `examples/01_basic_bringup_cli/main.cpp:324` reads persistent bytes through ordinary bounded jobs. CAN must distinguish callback/service acknowledgement, active state, persistence and cleanup; this RTC's safe cleanup writes and concrete wait times are not Lichuan behavior. |
| FieldCore commissioned interpretation | `include/TunnelMonitor/device/Rs485MotorModule.h:10` says commissioning supplies facts explicitly; `Rs485MotorModuleConfig` retains target, axis, prerequisites, reference and frozen read configuration separately. `src/devices/rs485_motor/Rs485MotorModule.cpp:50` requires a newer target generation on reconfiguration; `:109` forbids runtime retry and distinguishes UART changes from commissioning. `:512` compares newly read raw config against the frozen observation before state renewal; `:555` retains unknown effects and revokes dependent readiness/reference. `test/native/test_owner_rs485/test_rs485_task_fake.cpp:3524` checks recovery invalidates qualified reconciliation. `src/devices/i2c/Ina228Module.cpp:827` projects PARTIAL/INDETERMINATE into backend evidence. Board ADC channel/gain choices are visibly application policy at `src/devices/i2c/Ads1115Module.cpp:12`, not factory readback. CAN should retain commissioned source identity, observed configuration and application choices separately, without importing FieldCore types or changing this sibling. |

The applicable workflow/runner seams were inspected: RS
`.github/workflows/verify.yml` invokes `scripts/verify.py` with Python 3.12,
GCC/Clang native consumers and separate firmware jobs; I2C `ci.yml` files register
native, parser/contract, package and Doxygen checks; FieldCore
`.github/workflows/verify.yml:109` delegates its gates to `scripts/verify.py`.
These unchanged suites were not repeated for this documentary source review.

## Intake-validator findings for the lead

The existing `scripts/check_lichuan_ledger.py` correctly protects raw intake,
but intentionally rejects prompt-3 semantics: `:302` requires null reviewed
access/PDO permission, `:328` requires pending disposition, `:334` requires null
units/scaling/effect/edge and empty prerequisites, `:371` keeps manual items
pending, and `:484` rejects every resolved conflict. Its adversarial tests
explicitly exercise those intake restrictions. Do not remove raw-source
integrity checks merely to admit reviewed policy.

Minimal extension recommendations, not changes made by this review:

- Introduce an explicit reviewed phase or equivalent schema distinction while
  retaining immutable raw EDS accounting, source hashes, field omissions,
  expression bytes and original claim references.
- Validate reviewed semantic fields with finite dispositions and structured
  source references. Distinguish documentary review completion from firmware
  qualification and unsupported actual capability. A prose decision alone is
  not writable authorization.
- Require reviewed write permission to have established type/width/encoding,
  effect, model scope, prerequisites and bounded owner. Unknown affected effect
  or type, reserved entries and unresolved conflicting write semantics must
  fail closed. Preserve independently safe read scope; do not block unrelated
  fields by a whole-object conflict when the conflict is narrower.
- Validate field-specific conflict disposition, retained competing claims,
  precise dependent-feature gate and next evidence. A conservative policy can
  be reviewed while the manufacturer contradiction stays unresolved; do not
  fabricate a winning source to make the review complete.
- Narrow ownership according to prompt 3's actual groups instead of accepting
  any integer `2..50`. Add adversarial cases for a writable unknown effect,
  fake structured evidence, lost competing claim, wrong model/firmware binding,
  action misclassified as a harmless setting, consuming read, and documented
  evidence incorrectly promoted to implementation or hardware qualification.

No generic transaction/effect framework is needed for this stage. Keep one
ledger, one existing validator and independently derived negative cases.
The lead owns integration, final local integrity/ledger tests, documentation
links and Git synchronization. No installed API, example or CHANGELOG behavior
change arises from this read-only report; Doxygen is N/A.
