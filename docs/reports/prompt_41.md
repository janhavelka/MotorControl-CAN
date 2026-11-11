# Prompt 41 typed device and network console

Implementation on `promptset/motorcontrol-can`, entry/base checkpoint
`a4bfc0802cc76454a4cc4ccd883891a598cd880b`, upstream
`origin/promptset/motorcontrol-can`. Promptset v2 base remains `7878f7c`.
The entry fetch found 0 ahead / 0 behind and a clean tracked tree. Three unrelated
untracked `.vscode` files were preserved. The lead alone performs Git operations.

## Delivered boundary

The [canonical console inventory](../../examples/common/ProbeConsole.cpp) and
[CLI contract](../cli_contract.md#typed-device-increment-prompt-41) own exact
syntax, effects, output fields, capacities and remaining gates. There is one
parser/application, one CAN owner, no installed controller or raw dictionary route.
All work here was native/fake or compile-only; no controller, serial port, motor,
physical save/factory restore or firmware flashing was exercised.

| Owned work | Actual implementation and independently tested path |
| --- | --- |
| Typed probe/identity/config/state and native group read routes | Whole-plan DEC-1 policy in ProbeApp before reservation/admission; existing public preparation/getters; exact values/provenance streamed before retained terminal. Config and unaccepted groups reject Q20 before any first field. |
| Conditional native settings | ProbeSettings over actual AxisSession/owner; whole-candidate validation, actual prior/state/control evidence, exact possible-TX notice, owner epoch migration, typed readback, partial masks and uncertainty. Includes driver parameters, 2202 selector, DI and 2500; no DO/filter/tuning guess. |
| Recorded motion profile | Existing ConfigurationRecord/captureConfiguration/prepareConfigurationRestore; local inspect/forget, exact external restoration plan through the same settings bridge. Only 6081/83/84, no full backup or save. |
| Explicit communication/persistence/PDO requests | ProbeCommissioning consumes copied concrete public preparations, same actual owner/session/target, exact handoff guards and externally supplied one-use permissions. Preserves original ACK/abort, lost replies, partial PDO prefix, unknown restart/durability and separate verification; never installs/qualifies PDO samples. |
| Selection/discovery/inventory | Existing configured slots only; separate subaxis, protocol-local endpoint range, finite current-bitrate DiscoveryScan, per-target findings and retained inspection. No route allocation, assumed fresh session, model upgrade or automatic selection. |
| CANopen network | Explicit addressed/broadcast NMT through retained owner APIs; cached network/heartbeat/PDO facts preserve requested/observed/serviceability/sample distinctions. NMT TX never becomes remote confirmation or motor stop. |
| Diagnostics/monitoring/capture | Finite STATUS monitoring with frozen binding/sample identities, fixed total cutoff and output-paced next admission; passive checked trace copies, cached load, existing externally planned DeviceEvidence capture. No second RX or hidden traffic. |
| Correlation/portability | Capacity precedes admission; frozen request/operation/mode and native-context identity, exact same owner/store/session graph. Native example, isolated application consumer and both SDK manifests compile shared sources; core/export identity unchanged. |

External application code must establish native prerequisites/review, grant each
exact reviewed upload/download, and wire the exact `beforeTransmit` guard. None
is manufactured by CLI. Ordinary firmware has no arming or grant command. A
native conditional workflow can be implemented while its default-console or
physical capability remains gated. Source conflicts, node/bitrate/LSS/general
remapping, DO/tuning actions, durable saved scope and parameter-reload behavior
retain their named source/firmware owners. Motion/actions and full platform parity
remain prompt 42. The [source audit](../audit/PROMPT_41_SOURCES.md) records primary
pages and current sibling state; no reference snapshot or ledger fact was changed.

## Independent review and fixes

Parallel read-only source/architecture review, independent owner/fake vectors and
stronger final correlation/commissioning review inspected the actual tree.
Confirmed issues corrected during integration:

- Whole-plan read effects were previously enforced only by DeviceEvidence.
  Ordinary console calls now reject any unaccepted field before reserving work.
- Ordinary cancellation used to interrupt in-flight service and quarantine it.
  It now settles the exact current field then withdraws the remainder. Explicit
  diagnostic experiment interruption keeps its stronger original cancellation
  semantics. Existing DeviceEvidence regression assertions were preserved.
- Inactive firmware stranded accepted recovery. It now progresses only bounded
  host recovery/platform work and refuses any controller restart while inactive.
- Monitoring could adopt a foreign between-sample read or report success after
  omitting samples at its cutoff. Binding/sample IDs, ownership exclusion and a
  separate logical monitor outcome now prevent both; off is local cancellation.
- Passive CLI discovery now preserves the native network-only route path; it
  does not require or fabricate an SDO session.
- Discovery interface changes were first checked after owner service. The shared
  application now withdraws stale-interface scan work before owner progression.
- Typed helper attachment and request routing now validate actual owner/store/
  session identity plus node generation and both COB IDs, not numeric bindings
  alone. Replacement contexts cannot be advanced, cancelled or released under an
  old console operation, including direct feed before polling.
- Debug now decodes checked heartbeat/EMCY fields rather than applying a decoded
  label to raw bytes. Discovery findings and capture values/metadata are exposed.
- Static argument parsing accepts valid slot/subaxis zero independently from the
  strictly nonzero request/result ID parser. Source-gate Status/detail survives
  console rejection instead of being mislabeled a syntax or protocol failure.
- Local/device dispatch frames were separated and settings preparation uses
  unretained owned context storage, bringing strict target frames/chains below
  the existing limits without exemptions, larger task stacks or weaker checks.

The initial quick run caught the diagnostic-interruption compatibility issue;
its failed log remains in `.build/p41-quick`. A native commissioning test initially
used a heartbeat older than its reservation and correctly failed preparation;
the fixture now supplies actual heartbeat capture at the reservation boundary.
No production gate or timing rule was relaxed for either failure.

## Verification and handoff

| Gate | Actual result / retained evidence |
| --- | --- |
| Central full GCC verification | Passed: 123/123 registered native/Python contracts, isolated package suite and strict Doxygen. `.build/p41-full/summary.json` identifies every command/tool/version/status/log. |
| Final integrated rebuild/contract pass | Passed again after final correlation/model checks:123/123, `.build/p41-closure-build.log`, `.build/p41-closure-contracts.log`. |
| Final package consumers | Passed, `.build/p41-closure-package.log`: independent public headers, C++11 library, C++17/no-RTTI consumers, relocated installed/exported source and isolated native application, missing-dependency negatives and SDK-free IDF CMake boundary. |
| JSONL/native entry | Real transcript tests passed, including explicit host selection, one probe value, nine identity values, retained result/release and Q20 config rejection. Complete records stay below 512 bytes. |
| Arduino 3.3.11 | Compile/link and actual shared-source inventory passed, `.build/p41-final-arduino/summary.json`; no ports/flashing. |
| ESP-IDF 5.5.5 S2 | Compile/link passed, `.build/p41-final-idf/summary.json`; all console/device/settings/commissioning/shared-owner sources present. No ports/flashing. A mistaken attempt to build the retained artifact directory failed because the checker intentionally retains no CMake cache; the real checker was rerun and passed. |
| Xtensa S2/S3 at -Os/-Og | Strict measurement passed without baseline exemptions: no frame>2048 or known chain>6144. `.build/p41-final-stack/report.json`; unknown/indirect/libc/SDK edges remain explicitly unmeasured, so chain totals are lower bounds, not runtime stack guarantees. |
| Planning/repository/generated/diff | Passed actual repository registration, version/object freshness, source-manifest tests, promptset integrity and whitespace checks. Original references and generated facts unchanged. Staged/HEAD integrity checks accompany commit closure. |

Measured optional target storage (S2/S3 ABI): ProbeApp 2360 bytes,
ProbeConsole 3816, ProbeDevice 6664, ProbeSettings 2928, ProbeCommissioning 7432.
The last two are optional stationary caller storage, not allocated in every
firmware. No heap, second receiver or installed lifecycle was added. The console
retains two operations, two ordinary and two reserved-control records; line 256,
record 512, input budget 32 bytes/turn. The ordinary owner retains its existing
capacities/budgets. Native fake batch remains 4096 input bytes / 65536 output bytes
and 2000000 turns. Runtime firmware high-water/electrical/service-gap qualification
was not performed.

**Synchronization and remote verification.** Commit and push use the existing
execution branch. Required automatic GitHub Actions must be checked on the exact
final commit before handoff; its commit/run URL and current 5-job result are in the
final handoff and retained `.build/p41-ci` receipt. An earlier green run is not
substituted. No workflow trigger, dependency pin or test gate was weakened.

**Next executable work.** Prompt 42 integrates real finite motion/action routes,
reserved stop behavior, simple-move intent and full route/platform parity through
this same inventory/application. Q20 acquisition, model/firmware source conflicts,
PDO sample provenance and persistence physical evidence stay with their existing
owners; a console command does not close them.
