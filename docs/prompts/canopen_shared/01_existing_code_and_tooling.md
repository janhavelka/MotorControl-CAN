# 01 — Existing code and tooling for a new profile

Execute this stage under the family's execution contract.
Status: NOT STARTED. The inventory below was taken at `5045423d` on 2026-11-19, with
prompt 56 complete and prompts 57 and 63-78 pending. It is a starting map; the
current checkout decides. Read [ADD_DRIVE.md](../ADD_DRIVE.md), [COMMON.md](../COMMON.md),
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
| Identity | `Endpoint.h` | `Protocol`, `Endpoint`, `Binding`, `Generation`, `validateBinding`, `sameBinding` | `Endpoint.profile` is an application number, not `DriveProfile` |
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
| PDO | `canopen/Pdo.h`, `canopen/PdoState.h` | `PdoProfile`, `PdoObject`, `validatePdoMap`, `PdoRoute`, `decodePdo`, `pdoMapFromReadback`, `PdoRecord`, `pdoFreshness` | `PdoProfile` is call-only: the family supplies its set and slot counts and objects |
| SYNC | `canopen/Sync.h` | `makeSyncRequest`, `checkSyncFrame`, `prepareSync`, `nextSync`, `advanceSync`, `cancelSync` | Synchronous PDO behavior is per-family evidence |
| Service engine | `adapters/canopennode/SdoClient.h`, `Nmt.h`, `NetworkState.h`, `Capacity.h`; [stack decision](../../decisions/canopen_stack.md) | `SdoClient`, `MOTORCONTROLCAN_SDO_CHANNELS`, `MOTORCONTROLCAN_PDO_ROUTES` | One backend for every CANopen family, one capacity set per program; best-effort stop shape is seam S3 |
| Owner | `adapters/owner/CanBusOwner.h`, `CanBusTypes.h` | `beginSequence`, `beginOperation`, `admit`, `admitUrgent`, `admitBestEffortStop`, `service`, `result`, `release`, `cancel`, `recover`, `invalidate`, `invalidateProducer`, `health`, `driverState`, `stats`, `setSdoReuse`; `Transport`, `ServiceBudget`, `BusRequest`, `Completion`, `Health`, `Stats`, `SdoReuse` | No profile code; `SdoReuse()` default markers 1000:00 and 1018:01-03 rest on Lichuan DEC-1 |
| Transport | `adapters/twai/TwaiAdapter.h`, `IdfTwaiDriver.h` | Board adapter | Frame and controller facts only |
| Test plumbing | `test/support/OwnerRig.h` | `OwnerRig::route`, `OwnerRig::frame` | Generic fake-owner plumbing |

### Reuse the convention, not the content: tooling and packaging

| Concern | Lichuan instance | What the family copies |
| --- | --- | --- |
| Object ledger | `docs/reference/lichuan_objects.json` (schema 2), `scripts/check_lichuan_ledger.py` `validate_ledger` | Schema, lossless accounting, retained conflicts; not `MODELS`, `WITHHELD_GENERATION_KEYS`, `ORIGINAL_CONFLICT_CLAIMS_SHA256` or Q/C/DEC identifiers |
| Generated headers | `scripts/generate_lichuan_objects.py --check`; `Types.h`, `Objects.h`, `Catalogue.h`, `Pdo.h`, `lichuan_generated.md` | Deterministic check mode, LF rules in `.gitattributes`, optional catalogue strings |
| Operation map | `docs/reference/lichuan_operations.json`, `scripts/check_lichuan_operations.py` | Family/function/test/console schema; its header scan covers every `include/MotorControlCAN` header (seam S7) |
| Source manifest | `docs/reference/SOURCES.json`, `multidrive/SOURCES.json`; `scripts/check_promptset.py` `check_records` | Shape and hash rules; `reference_manifests` is a hard-coded list |
| Registration | `test/CMakeLists.txt` `strict_test`/`add_test`; `scripts/check_repository.py`; `scripts/verify.py` steps `objects`, `operations` | Explicit `add_executable`/`add_test` per file; `check_repository.py` `registration_checks` fails an unregistered `test/*_test.cpp` or `*_test.py` |
| Packaging | Root `CMakeLists.txt` `MOTORCONTROLCAN_SOURCES` and per-directory `install(FILES ...)`; `library.json` `export.include`; `test/package_test.py` `HEADERS`; `test/installed_consumer/`, `test/idf_consumer/` | Explicit lists; vendor files never exported |
| Documentation | `Doxyfile` (`include` recursive, strict warnings) | New public headers are covered automatically and must pass |

### Lichuan-specific: never reuse

- Everything in `MotorControlCAN::LICHUAN_C` (`include/MotorControlCAN/profiles/lichuan_c/`,
  `src/profiles/lichuan_c/`), for example `StatusLayout` and `interpretStatusword`
  (manual dialects EN86/ZH86/ZH57/OP86), `MONITOR_*` (object 2100), `STATUS_EN86_ZERO_SPEED`,
  `StopKind` (010F halt, 000B quick stop), `stopPredecessorAllowed` (DEC-8),
  `AlarmClearMethod` (2301, 6040 bit 7), `prepareSetDevicePosition` (2302), `IdentityWidths`
  (1009/100A), `prepareSave`/`prepareFactoryRestore` (2300), `EnableGate`,
  `ENABLE_SERVICE_STEPS`, mode-6 completion on statusword bit 8, `ReadPlan.h`, `ProfileEvidence.h`.
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
protocol domain per interface unless prompt 63's analysis admits sharing.

## Deliver

- The family's inventory record in its stage report: each row confirmed, changed or
  absent at the recorded HEAD, its classification, and the seams needed with their
  current state in the [README table](README.md).
- The MotorControl-RS audit row and any FieldCore-node boundary note (read-only).
- No source change. A seam is implemented only by the family stage or main-sequence
  prompt that owns it.

## Acceptance

Every reused symbol was read with its callers at the recorded HEAD. The protocol choice
cites family evidence. Nothing listed as Lichuan-specific is planned as reuse. Needed
seams name their owner. No implementation is claimed. Local checks:
`python scripts/check_promptset.py` and `git diff --check`.
