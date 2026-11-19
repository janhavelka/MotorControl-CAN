# 01 — Existing code and tooling for a new profile

Execute this stage under the family's execution contract.
Status: NOT STARTED. The inventory below was taken at `5045423d` on 2026-11-19, with
prompt 56 complete and prompts 57 and 63-78 pending; the owner, source-manifest,
packaging and `StopKind` rows were rechecked at `f3c3870a` ([audit application](audit_application.md)), and the
PDO, SYNC, service engine, owner and packaging rows at `139748e9` (round 2).
It is a starting map; the current checkout decides. Read [ADD_DRIVE.md](../ADD_DRIVE.md), [COMMON.md](../COMMON.md),
[CONVENTIONS.md](../CONVENTIONS.md) sections 1-4 and 6, [MULTIDRIVE.md](../MULTIDRIVE.md),
the [multi-drive decision](../../multidrive_decision.md), the [profile contract](../../profile_contract.md)
and [PROGRESS.md](../PROGRESS.md) first.

## Work

1. Record HEAD, dirty state and PROGRESS rows 57, 63, 64, 65, 73, 75 and 77. Refresh
   MotorControl-RS HEAD and its profile seam (`docs/prompts/lichuan_shared/`,
   `docs/profile_contract.md`) under AGENTS' recurring audit; name each adopted or
   rejected behavior with path and symbol.
2. Establish the family's protocol from its own manuals/EDS before choosing a column
   below. A CAN connector, an object number or a vendor's "SDO" label is not evidence.
3. For every row, open the file, read the symbol and its callers, and record: confirmed,
   delta (what changed) or absent. A grep hit alone does not confirm meaning.
4. Classify each item for the family: reuse as is, extend a shared seam (README table),
   replace with family code, or not applicable. Name the seams the family will need.

### Reuse as is: installed core

| Area | Path | Symbols | Limit |
| --- | --- | --- | --- |
| Call result | `include/MotorControlCAN/Status.h` | `Status`, `Err`, `Ok`, `errToString` | `PROTOCOL_ERROR` is a checked native rejection; add no `Err` without a failing caller |
| Identity | `Endpoint.h` | `Protocol`, `Endpoint`, `Binding`, `Generation`, `validateBinding`, `sameBinding` | `Endpoint.profile` is an application number, not `DriveProfile`, until seam S2 |
| Capability facts | `Capabilities.h` | `MotorKind`, `FeedbackKind`, `Capabilities`, `Capability`, `requireCapability` | Caller-supplied facts, never detection |
| Units, coordinates | `Units.h`, `Axis.h` | `Rational`, `UnitConfig`, `convertDisplacement`, `convertVelocity`, `convertAcceleration`, `quantizeInteger`, `quantizeFloat`, `parseExactNumber`, `AxisConfig`, `preparePosition`, `prepareVelocityTarget`, `configureAxis`, `setAxisOrigin`, `invalidateAxisReference` | `NativeRateBasis` offers `COMMAND_INCREMENTS` and `MOTOR_TURNS` only |
| Native width, ramp | `Native.h` | `NativeRamp`, `RampKind`, `convertNativeRamp`, `checkNativeInteger` | A ramp time never becomes an acceleration |
| Operation vocabulary | `ReadOperation.h`, `ActionOperation.h`, `MoveOperation.h`, `VelocityOperation.h`, `HomeOperation.h` | `ReadState`, `ReadOutcome`, `ActionState`, `ActionExecution`, `ActionCompletion`, `ActionOutcome`, `RetryClass`, `ServiceEvidence`, `checkServiceEvidence`, `MoveRequest`, `MoveInterruption`, `MoveRelation`, `FiniteVelocityRequest`, `VelocityInterruption`, `HomeInterruption` and the `*Error` detail enums | `VelocityError::RATE_BASIS` and `HomeError::RATE_BASIS` comments describe Lichuan's rpm destination |
| Frames | `can/Frame.h` | `Frame`, `validateFrame` | Classical CAN only; FD is its own milestone |
| Inventory | `Discovery.h`, `src/Discovery.cpp` | `Manufacturer`, `DriveProfile`, `DiscoveryProfile`, `DiscoveryCapabilities`, `getDiscoveryProfile`, `getDiscoveryCapabilities` | One entry; seam S1 |
| Private helpers | `src/ExactArithmetic.h`, `src/canopen/SdoEvidence.h` | Checked arithmetic, SDO evidence copies | Not installed; usable from `src/` only |

### Reuse as is: CANopen services below common motion

| Area | Path | Symbols | Limit |
| --- | --- | --- | --- |
| SDO codec | `include/MotorControlCAN/canopen/Sdo.h` | `SdoChannel`, `SdoExpectation`, `checkRequest`, `checkReply`, `readWord`, `readSignedWord`, `validateSdoChannel` | Checks requests and initiation replies; segment transfer runs in CANopenNode behind `SdoClient` |
| SDO intent, evidence | `canopen/SdoTransfer.h` | `SdoTransfer`, `SdoCompletion`, `SdoReadEffect`, `SdoOutcome`, `repeatableReadEffect` | Each upload's `readEffect` comes from the family ledger; Lichuan DEC-1 does not transfer |
| NMT, heartbeat, EMCY | `canopen/Network.h` | `makeNmtRequest`, `NmtCommand`, `NmtState`, `NetworkRoute`, `checkNetworkEvent` | NMT stop is not a quick stop; EMCY meaning is per family |
| PDO | `canopen/Pdo.h`, `canopen/PdoState.h` | `PdoProfile`, `PdoObject`, `validatePdoMap`, `PdoRoute`, `decodePdo`, `pdoMapFromReadback`, `PdoRecord`, `pdoFreshness` | `PdoProfile` is call-only: the family supplies its set and slot counts and objects. Map review and decoding only; `PdoObject` grants no RPDO TX (seam S14) |
| SYNC | `canopen/Sync.h` | `makeSyncRequest`, `checkSyncFrame`, `prepareSync`, `nextSync`, `advanceSync`, `cancelSync` | Synchronous PDO behavior is per-family evidence; the owner's `armSync` leaves the remote buffered RPDO hazard to the caller (seam S13) |
| Service engine | `adapters/canopennode/SdoClient.h`, `Nmt.h`, `NetworkState.h`, `Capacity.h`; [stack decision](../../decisions/canopen_stack.md) | `SdoClient`, `MOTORCONTROLCAN_SDO_CHANNELS`, `MOTORCONTROLCAN_PDO_ROUTES` | One backend for every CANopen family, one capacity set per program; `validBestEffortStop` accepts `6040:00=000B` on every channel (seam S3) |
| Owner | `adapters/owner/CanBusOwner.h`, `CanBusTypes.h` | `beginSequence`, `beginOperation`, `admit`, `admitUrgent`, `admitBestEffortStop`, `service`, `result`, `release`, `cancel`, `recover`, `invalidate`, `invalidateProducer`, `health`, `driverState`, `stats`, `setSdoReuse`; `Transport`, `ServiceBudget`, `BusRequest`, `Completion`, `Health`, `Stats`, `SdoReuse` | No profile code. Admits SDO work (`BusRequest` carries an `SdoTransfer`), best-effort stops, drain markers, NMT and SYNC; no PDO data frame (seam S14). `SdoReuse` is per configured channel binding; every channel starts with `SdoReuse()` (ACCEPTED_RISK_WITH_MARKER, markers 1000:00 and 1018:01-03 on Lichuan DEC-1), so `resyncRoutes` sends a marker after the route's first timeout, cancellation or bus-off with possible TX, without any application call (seam S10) |
| Transport | `adapters/twai/TwaiAdapter.h`, `IdfTwaiDriver.h` | Board adapter | Frame and controller facts only |
| Test plumbing | `test/support/OwnerRig.h` | `OwnerRig::route`, `OwnerRig::frame` | Generic fake-owner plumbing |

### Reuse the convention, not the content: tooling and packaging

| Concern | Lichuan instance | What the family copies |
| --- | --- | --- |
| Object ledger | `docs/reference/lichuan_objects.json` (schema 2), `scripts/check_lichuan_ledger.py` `validate_ledger` | Schema, lossless accounting, retained conflicts; not `MODELS`, `WITHHELD_GENERATION_KEYS`, `ORIGINAL_CONFLICT_CLAIMS_SHA256` or Q/C/DEC identifiers |
| Generated headers | `scripts/generate_lichuan_objects.py --check`; `Types.h`, `Objects.h`, `Catalogue.h`, `Pdo.h`, `lichuan_generated.md` | Deterministic check mode, LF rules in `.gitattributes`, optional catalogue strings |
| Operation map | `docs/reference/lichuan_operations.json`, `scripts/check_lichuan_operations.py` | Family/function/test/console schema. `check` requires every `include/MotorControlCAN` header with public functions to be a mapped family, so the first new header fails it (seam S7) |
| Source manifest | `docs/reference/SOURCES.json` and `docs/reference/<pack>/SOURCES.json`; `scripts/check_promptset.py` `check_records` | Shape and hash rules. Every immediate pack manifest is discovered by glob (`reference_manifests`); records may cite files anywhere in `docs/reference`; each artifact's size and SHA-256 are checked, and the Git blob with `--check-git-ref`. Only `multidrive`, `other_can` and `public_code` are required by name |
| Registration | `test/CMakeLists.txt` `strict_test`/`add_test`; `scripts/check_repository.py`; `scripts/verify.py` steps `objects`, `operations` | Explicit `add_executable`/`add_test` per file; `check_repository.py` `registration_checks` fails an unregistered `test/*_test.cpp` or `*_test.py` |
| Packaging | Root `CMakeLists.txt` `MOTORCONTROLCAN_SOURCES` and per-directory `install(FILES ...)`; `library.json` `export.include`; `test/package_test.py` `HEADERS` and exact archive set; `test/installed_consumer/`, `test/idf_consumer/` | `export.include` names `include/` and `src/` as directories that `scripts/export_package.py` walks recursively, and `check_repository.py` `EXPORTS` requires exactly that list: a family edits neither, and everything under `include/` and `src/` ships, so vendor files never go there. CMake sources, install rules and `package_test.py` sets are explicit per family, so a new header enters the archive at once and fails the exact archive set until `HEADERS` names it, then the install set until an install rule ships it. `test/installed_consumer/CMakeLists.txt` compiles each header alone (`headers_11`, `headers_17`) by glob, in the source and the installed consumer |
| Documentation | `Doxyfile` (`include` recursive, strict warnings) | New public headers are covered automatically and must pass |

### Intake rules for the family's evidence and tooling stages

These bind every family stage that adds sources, a ledger, generated output or the first
public header. Stage 01 itself changes no source.

- **Pack.** `docs/reference/<family>/SOURCES.json` shaped like
  [multidrive/SOURCES.json](../../reference/multidrive/SOURCES.json) (per source `url`,
  `retrieved_on`, `bytes`, `sha256`, `applicability`, `firmware_match`, `license`,
  `revision`, `pdf_pages`; `remote_only` entries with no path, hash or bytes), unmodified
  originals in `vendor/`, extracts in `extracted/` with one-based PDF pages. The checker
  discovers the manifest; do not edit `reference_manifests`. Run
  `python scripts/check_promptset.py` on the working tree, with `--check-git-ref INDEX`
  before the commit and `HEAD` after. If the pack's absence must fail the planning check,
  add it to the required-pack list with a negative test; discovery alone requires nothing.
  Never run vendor tools. Vendor material is outside the MIT license and never enters
  `include/`, `src/`, an install rule or a packed archive.
- **CANopen ledger.** `docs/reference/<family>_objects.json`, schema 2, with the top-level
  keys and entry fields of `lichuan_objects.json`: per entry `key`, `scope_id`, `type`, EDS
  and reviewed access and PDO mapping, `units`, `scaling`, `effect`, `prerequisites`,
  `source_refs` (PDF page or EDS line), `conflict_ids`, `coverage` (independent
  `documented`, `implemented`, `host_tested`, `hardware_qualified`) and `review`
  (`read_effect`, `write_policy`, `gate_ids`). Prose-only functions go in `manual_only`;
  `conflicts` keep every original claim under a hash guard; `owner_prompt` names the
  family's stage IDs. No EDS default is written at initialization; an RW flag grants no write.
- **Other-protocol ledger.** The same accounting (`key`, `source_refs`, `units`, `scaling`,
  `effect`, `conflicts`, `coverage`, `review`) over the protocol's own items: message or
  command identifiers, frame format (11/29-bit, RTR, DLC, FD), signal layout, byte order,
  acknowledgement or no-ack, refresh and watchdog. No EDS, PDO or object-dictionary fields.
- **Operation map and tools.** `docs/reference/<family>_operations.json`, schema 2, with
  Lichuan's keys (`families` with `header`, `functions`, `tests`, `console`;
  `ledger_bindings`; `python_plans`); it references the ledger and never copies device
  facts. `scripts/check_<family>_ledger.py`, `scripts/generate_<family>_objects.py`
  (`--check` never writes; outputs `Types.h`, `Objects.h`, optional `Catalogue.h`, `Pdo.h`
  where applicable, `docs/reference/<family>_generated.md`) and
  `scripts/check_<family>_operations.py`, each with a registered negative test: missing row,
  tampered hash, altered output, unclassified function, promoted gate. LF rules for
  generated headers go in `.gitattributes`; extend `TYPE_CODES` only from the family EDS, or, where the vendor publishes no EDS, from the
  vendor's firmware specification, recorded as such per entry until an EDS confirms it.
- **Seam S7, before or in the same commit as the first new public header.** The stage that
  adds it claims S7 (README), scopes `check_lichuan_operations.py` to common and
  `lichuan_c` headers and the family checker to `profiles/<family>/`, and tests that the
  checkers' header sets are disjoint, their union is every public header with public
  functions, and an unclassified common, Lichuan or family function still fails. Register
  each checker in `scripts/verify.py`, `scripts/check_repository.py` (`CHECKS`, file
  lists) and CTest; that registration is itself an S7 edit, so a stage that registers a
  checker before its first header claims S7 first. The commit that adds the first header
  also adds its `install(FILES ...)` rule, its `test/package_test.py` `HEADERS` and exact
  archive entries (with any new `src/` file) and passes the per-header consumer compile and
  `test/package_test.py`; no commit ships a header that the package gates do not name.
  Registration later consumes the checked ledger, codec, partition and package entries.

### Lichuan-specific: never reuse

- Everything in `MotorControlCAN::LICHUAN_C` (`include/MotorControlCAN/profiles/lichuan_c/`,
  `src/profiles/lichuan_c/`), for example `StatusLayout` and `interpretStatusword`
  (manual dialects EN86/ZH86/ZH57/OP86), `MONITOR_*` (object 2100), `STATUS_EN86_ZERO_SPEED`,
  `StopKind` (mode- and predecessor-specific, see its declaration in `Actions.h`: HALT is
  010F in modes 1/3 and a derived 000F from acknowledged 001F in mode 6; QUICK_STOP is
  000F to 000B; allowed predecessors per `stopPredecessorAllowed`, DEC-8), kept apart from
  the unresolved-route best-effort stop (DEC-2), `AlarmClearMethod` (2301, 6040 bit 7),
  `prepareSetDevicePosition` (2302), `IdentityWidths` (1009/100A),
  `prepareSave`/`prepareFactoryRestore` (2300), `EnableGate`, `ENABLE_SERVICE_STEPS`,
  mode-6 completion on statusword bit 8, `ReadPlan.h`, `ProfileEvidence.h`.
  Copy a mechanism only when both callers need identical behavior (seam S8).
- Ledger decisions and gates: DEC-1 read acceptance, DEC-2, DEC-8, Q01-Q28, conflict IDs,
  scope `eds_family_unbound`, bench states S20/S21 and the CL86-C pulse-rate limit on `2201`.
- Application semantics: `MotionIntent` (0..3000 motor rpm, 0..2000 ms ramps); console verbs
  `subdivision`, `stepsperturn`, `speed`, `accel`, `decel`, `io set`, `control`, `tuning`,
  `home METHOD ...`, `alarm-clear`, `position-clear`, `pdo apply map3-to-map2`,
  `communication heartbeat 1000`, `persistence`, `motion-profile`; `FakeTransport` in
  `examples/native_probe/` and `test/support/BenchDrive.h` responses.
- Bench facts: S2 on COM15, nodes 1 and 2, image `d89fddd4`, and the object lists in
  COMMON's owner-absence rules.

### Shared code that still assumes Lichuan

`AxisSession`, `ProbeApp`, `ProbeMotion`, `ProbeSettings`, `MotionIntent` and
`DiscoveryScan` in `examples/common/` are typed on `LICHUAN_C` (prompt 64 owns that
change). `ProbeConsoleText.h` writes `profile:"lichuan_c"` into every JSONL record and
`scripts/console_jsonl.py` rejects any other `PROFILE`. `DiscoveryProfile`'s default
member values are Lichuan's. `docs/coverage.md`, `docs/api_cli_mapping.md` and
`docs/profile_contract.md` describe Lichuan only. Treat each as a seam, not as reuse.

### A non-CANopen CAN protocol instead

Reuse `Status`, `Endpoint` with `Protocol::PROPRIETARY`, `Capabilities`, units and axis
preparation, `Native.h`, the operation vocabularies (including `checkServiceEvidence`
with `AcknowledgementSupport::NONE`), `Frame` (11/29-bit, RTR, DLC) and the owner's
`Transport`. Do not use `canopen/`, `SdoClient`, `SdoReuse`, NMT, heartbeat, PDO or a
statusword projection. Required instead, per [ADD_DRIVE.md](../ADD_DRIVE.md) and
[other protocols](../../audit/OTHER_CAN_PROTOCOLS.md) ("Required architecture
consequences"): the protocol's own bounded codecs and service contexts beneath common
motion; owner admission for its frames (today `BusRequest` carries only an
`SdoTransfer`; [prompt 63](../63_mixed_protocols.md) owns the identifier plan, receive
route and raw transmit kind); its own evidence classes for `Health`; explicit no-ack,
refresh-lease and watchdog semantics; FD as a separate adapter capability; one
protocol domain per interface unless prompt 63's analysis admits sharing. Until prompt
63's artifacts land and pass, the family's device-facing routes are Blocked; its codecs,
ledger and host codec tests are not.

## Deliver

- The family's inventory record in its stage report: each row confirmed, changed or
  absent at the recorded HEAD, its classification, and the seams needed with their
  current state and claims in the [README table](README.md).
- The MotorControl-RS audit row and any FieldCore-node boundary note (read-only).
- No source change in this stage. The intake rules bind the family stages that add
  sources, ledgers, generated output and headers; a seam is implemented only by the
  family stage that claimed it or the main-sequence prompt that owns it.

## Acceptance

Every reused symbol was read with its callers at the recorded HEAD. The protocol choice
cites family evidence. Nothing listed as Lichuan-specific is planned as reuse. Needed
seams name their owner. No implementation is claimed. Local checks:
`python scripts/check_promptset.py` and `git diff --check`.
